#include <basalt/server/assets/AssetCache.hpp>
#include <basalt/server/bukkit/BukkitEmbedded.hpp>
#include <basalt/server/runtime/bukkit/BukkitRuntimeFiles.hpp>
#include <basalt/wire/auth/HttpsClient.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>

namespace basalt::server::runtime::bukkit {
namespace {
namespace fs = std::filesystem;

constexpr std::string_view kCacheRoot = ".basalt/bukkit";
constexpr std::string_view kUnpackPrefix = "shim-";
constexpr std::size_t kMaxJarBytes = 128UL * 1024UL * 1024UL;

[[nodiscard]] bool isFile(const fs::path& path) {
    std::error_code error;
    return fs::is_regular_file(path, error);
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

[[nodiscard]] std::optional<std::string> unpack(std::span<const std::uint8_t> bytes, const fs::path& target) {
    const fs::path partial = fs::path{target}.concat(".part");
    if (!wire::io::fileIO::writeBytes(partial.string(), bytes)) {
        return "could not write " + partial.string();
    }
    std::error_code error;
    fs::rename(partial, target, error);
    return error ? std::optional{"could not move " + partial.string() + " into place"} : std::nullopt;
}

[[nodiscard]] std::expected<fs::path, std::string> unpackEmbedded(const server::bukkit::EmbeddedRuntime& embedded) {
    std::string identity = assets::AssetCache::sha1Hex(embedded.libjvm);
    identity += assets::AssetCache::sha1Hex(embedded.bootClasspath) + assets::AssetCache::sha1Hex(embedded.runtimeJar);
    const std::span<const std::uint8_t> identityBytes{reinterpret_cast<const std::uint8_t*>(identity.data()),
                                                      identity.size()};
    const fs::path directory = fs::absolute(
        fs::path{kCacheRoot} / (std::string{kUnpackPrefix} + assets::AssetCache::sha1Hex(identityBytes).substr(0, 16)));
    const std::array<std::pair<const char*, std::span<const std::uint8_t>>, 3> pieces{
        {{"libjvm", embedded.libjvm},
         {"classpath.jar", embedded.bootClasspath},
         {"basalt-bukkit.jar", embedded.runtimeJar}}};
    std::error_code error;
    fs::create_directories(directory, error);
    for (const auto& [name, bytes] : pieces) {
        if (fs::exists(directory / name, error) && fs::file_size(directory / name, error) == bytes.size()) {
            continue;
        }
        if (const std::optional<std::string> failure = unpack(bytes, directory / name)) {
            return std::unexpected(*failure);
        }
    }
    fs::permissions(directory / "libjvm", fs::perms::owner_exec | fs::perms::group_exec, fs::perm_options::add, error);
    for (const fs::directory_entry& entry : fs::directory_iterator(kCacheRoot, error)) {
        const std::string name = entry.path().filename().string();
        if (name.starts_with(kUnpackPrefix) && name != directory.filename().string()) {
            fs::remove_all(entry.path(), error);
        }
    }
    return directory;
}
} // namespace

std::expected<BukkitRuntimeFiles, std::string> locateRuntime(const wire::common::Logger& logger) {
    const server::bukkit::EmbeddedRuntime embedded = server::bukkit::embeddedRuntime();
    if (!embedded.complete()) {
        return std::unexpected("this build does not carry the Basalt Shim runtime (it needs a JDK and a working Shim "
                               "build when the server is compiled), so use an official release");
    }
    const std::expected<fs::path, std::string> directory = unpackEmbedded(embedded);
    if (!directory.has_value()) {
        return std::unexpected(directory.error());
    }
    BukkitRuntimeFiles files;
    files.libjvm = (*directory / "libjvm").string();
    files.bootClasspath = (*directory / "classpath.jar").string();
    files.runtimeJar = (*directory / "basalt-bukkit.jar").string();

    const fs::path cache = fs::path{kCacheRoot} / "lib";
    std::vector<Coordinate> missing;
    const std::vector<Coordinate> wanted = parseManifest(std::string{embedded.libraries});
    for (const Coordinate& coordinate : wanted) {
        if (!isFile(cache / coordinate.fileName())) {
            missing.push_back(coordinate);
        }
    }
    if (!missing.empty()) {
        std::error_code error;
        fs::create_directories(cache, error);
        logger.info([&missing] { return std::format("Fetching {} Bukkit libraries (once)...", missing.size()); });
        for (const Coordinate& coordinate : missing) {
            if (const std::optional<std::string> failure = download(coordinate, cache / coordinate.fileName())) {
                return std::unexpected(*failure + "; check the network connection and start the server again");
            }
        }
    }
    for (const Coordinate& coordinate : wanted) {
        files.libraries.push_back(fs::absolute(cache / coordinate.fileName()).string());
    }
    return files;
}
} // namespace basalt::server::runtime::bukkit
