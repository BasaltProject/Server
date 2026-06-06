#include <basalt/server/assets/AssetCache.hpp>
#include <basalt/server/runtime/bukkit/BukkitRuntimeFiles.hpp>
#include <basalt/wire/auth/HttpsClient.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <optional>
#include <string_view>
#include <system_error>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace basalt::server::runtime::bukkit {
namespace {
namespace fs = std::filesystem;

constexpr std::string_view kCacheRoot = ".basalt/bukkit";
constexpr std::string_view kManifest = "libs.txt";
constexpr std::size_t kMaxJarBytes = 128UL * 1024UL * 1024UL;

#if defined(__APPLE__)
constexpr std::string_view kLibjvm = "libjvm.dylib";
#else
constexpr std::string_view kLibjvm = "libjvm.so";
#endif

[[nodiscard]] fs::path executableDirectory() {
    std::error_code error;
#if defined(__linux__)
    const fs::path exe = fs::read_symlink("/proc/self/exe", error);
    return error ? fs::path{} : exe.parent_path();
#elif defined(__APPLE__)
    char buffer[4096];
    std::uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) != 0) {
        return {};
    }
    const fs::path exe = fs::weakly_canonical(buffer, error);
    return error ? fs::path{} : exe.parent_path();
#else
    return {};
#endif
}

[[nodiscard]] bool isFile(const fs::path& path) {
    std::error_code error;
    return fs::is_regular_file(path, error);
}

[[nodiscard]] std::optional<std::string> firstFile(const std::vector<std::string>& roots,
                                                   std::initializer_list<std::string_view> relatives) {
    for (const std::string& root : roots) {
        for (const std::string_view relative : relatives) {
            const fs::path candidate = fs::path{root} / relative;
            if (isFile(candidate)) {
                return fs::absolute(candidate).string();
            }
        }
    }
    return std::nullopt;
}

struct Coordinate {
    std::string group;
    std::string artifact;
    std::string version;

    [[nodiscard]] std::string fileName() const { return artifact + "-" + version + ".jar"; }

    [[nodiscard]] std::string url() const {
        std::string base = "https://repo1.maven.org/maven2";
        if (group == "com.mojang") {
            base = "https://libraries.minecraft.net";
        } else if (group == "io.papermc.paper") {
            base = "https://repo.papermc.io/repository/maven-public";
        }
        std::string path = group;
        std::ranges::replace(path, '.', '/');
        return std::format("{}/{}/{}/{}/{}", base, path, artifact, version, fileName());
    }
};

[[nodiscard]] std::vector<Coordinate> parseManifest(const std::string& text) {
    std::vector<Coordinate> found;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        std::string line = text.substr(start, end - start);
        start = end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        const std::size_t first = line.find(':');
        const std::size_t second = first == std::string::npos ? first : line.find(':', first + 1);
        if (line.empty() || line.front() == '#' || second == std::string::npos) {
            continue;
        }
        Coordinate coordinate{line.substr(0, first), line.substr(first + 1, second - first - 1),
                              line.substr(second + 1)};
        const bool seen = std::ranges::any_of(
            found, [&coordinate](const Coordinate& other) { return other.fileName() == coordinate.fileName(); });
        if (!seen) {
            found.push_back(std::move(coordinate));
        }
    }
    return found;
}

[[nodiscard]] std::vector<std::string> jarsIn(const fs::path& directory) {
    std::vector<std::string> jars;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator(directory, error)) {
        if (entry.path().extension() == ".jar") {
            jars.push_back(fs::absolute(entry.path()).string());
        }
    }
    std::ranges::sort(jars);
    return jars;
}

[[nodiscard]] std::optional<std::string> download(const Coordinate& coordinate, const fs::path& target) {
    const std::string url = coordinate.url();
    const auto jar = wire::auth::httpsGet(url, kMaxJarBytes);
    if (!jar.has_value() || !jar->ok()) {
        return "could not download " + url;
    }
    const auto checksum = wire::auth::httpsGet(url + ".sha1", 1024);
    if (!checksum.has_value() || !checksum->ok()) {
        return "could not download " + url + ".sha1";
    }
    const std::string published = checksum->text().substr(0, 40);
    if (assets::AssetCache::sha1Hex(jar->body) != published) {
        return "the SHA-1 of " + url + " does not match the published one";
    }
    const fs::path partial = fs::path{target}.concat(".part");
    if (!wire::io::fileIO::writeBytes(partial.string(), jar->body)) {
        return "could not write " + partial.string();
    }
    std::error_code error;
    fs::rename(partial, target, error);
    return error ? std::optional{"could not move " + partial.string() + " into place"} : std::nullopt;
}
} // namespace

std::vector<std::string> runtimeRoots(const config::BukkitConfig& config) {
    std::vector<std::string> roots;
    const auto add = [&roots](const fs::path& path) {
        if (!path.empty()) {
            roots.push_back(path.string());
        }
    };
    add(config.runtime);
    if (const char* home = std::getenv("BASALT_BUKKIT_HOME"); home != nullptr) {
        add(home);
    }
    add(kCacheRoot);
    add("runtime");
    if (const fs::path exe = executableDirectory(); !exe.empty()) {
        add(exe / "runtime");
    }
    return roots;
}

std::expected<BukkitRuntimeFiles, std::string> locateRuntime(const config::BukkitConfig& config,
                                                             const wire::common::Logger& logger) {
    const std::vector<std::string> roots = runtimeRoots(config);
    std::string searched;
    for (const std::string& root : roots) {
        searched += (searched.empty() ? "" : ", ") + root;
    }
    BukkitRuntimeFiles files;
    const auto pick = [&roots](const std::string& explicitPath, std::initializer_list<std::string_view> relatives) {
        return explicitPath.empty() ? firstFile(roots, relatives) : std::optional{fs::absolute(explicitPath).string()};
    };
    const std::optional<std::string> libjvm = pick(config.libjvm, {"shim/" + std::string{kLibjvm}, kLibjvm});
    const std::optional<std::string> boot = pick(config.bootClasspath, {"shim/classpath.jar", "classpath.jar"});
    const std::optional<std::string> runtimeJar = pick(config.runtimeJar, {"basalt-bukkit.jar"});
    if (!libjvm.has_value() || !boot.has_value() || !isFile(*libjvm) || !isFile(*boot)) {
        return std::unexpected(
            "the Basalt Shim runtime (" + std::string{kLibjvm} + " and classpath.jar) was not found in " + searched +
            ". Put both files in runtime/shim/ next to the server, or point $BASALT_BUKKIT_HOME or `runtime` in "
            "config/bukkit.toml at a folder that has them");
    }
    if (!runtimeJar.has_value() || !isFile(*runtimeJar)) {
        return std::unexpected("basalt-bukkit.jar was not found in " + searched +
                               ". This build was made without the Bukkit runtime; rebuild it with a JDK installed");
    }
    files.libjvm = *libjvm;
    files.bootClasspath = *boot;
    files.runtimeJar = *runtimeJar;

    if (!config.libraries.empty()) {
        files.libraries = jarsIn(config.libraries);
        return files;
    }
    std::vector<fs::path> libraryDirs;
    for (const std::string& root : roots) {
        libraryDirs.push_back(fs::path{root} / "lib");
    }
    const std::optional<std::string> manifest = firstFile(roots, {kManifest});
    const std::optional<std::string> manifestText = manifest ? wire::io::fileIO::readText(*manifest) : std::nullopt;
    const fs::path cache = fs::path{kCacheRoot} / "lib";
    std::vector<Coordinate> missing;
    for (const Coordinate& coordinate : parseManifest(manifestText.value_or(""))) {
        const bool present = std::ranges::any_of(libraryDirs, [&coordinate](const fs::path& directory) {
            return isFile(directory / coordinate.fileName());
        });
        if (!present) {
            missing.push_back(coordinate);
        }
    }
    if (!missing.empty()) {
        if (!config.download) {
            return std::unexpected(std::format("{} Bukkit libraries are missing (first: {}) and `download` is off in "
                                               "config/bukkit.toml",
                                               missing.size(), missing.front().fileName()));
        }
        std::error_code error;
        fs::create_directories(cache, error);
        logger.info([&missing] { return std::format("Fetching {} Bukkit libraries (once)...", missing.size()); });
        for (const Coordinate& coordinate : missing) {
            if (const std::optional<std::string> failure = download(coordinate, cache / coordinate.fileName())) {
                return std::unexpected(*failure + "; check the network or copy the jars into " + cache.string());
            }
        }
    }
    std::vector<std::string> seen;
    for (const fs::path& directory : libraryDirs) {
        for (std::string& jar : jarsIn(directory)) {
            const std::string name = fs::path{jar}.filename().string();
            if (std::ranges::find(seen, name) == seen.end()) {
                seen.push_back(name);
                files.libraries.push_back(std::move(jar));
            }
        }
    }
    return files;
}
} // namespace basalt::server::runtime::bukkit
