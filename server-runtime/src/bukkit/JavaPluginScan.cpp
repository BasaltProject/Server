#include <basalt/server/runtime/bukkit/JavaPluginScan.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/io/ZipReader.hpp>

#include <algorithm>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::bukkit {
namespace {
constexpr std::string_view kBasaltManifest = "plugin.toml";

[[nodiscard]] std::string valueOf(std::string_view yaml, std::string_view key) {
    std::size_t start = 0;
    while (start < yaml.size()) {
        const std::size_t end = std::min(yaml.find('\n', start), yaml.size());
        std::string_view line = yaml.substr(start, end - start);
        start = end + 1;
        if (!line.starts_with(key) || line.size() <= key.size() || line[key.size()] != ':') {
            continue;
        }
        line.remove_prefix(key.size() + 1);
        while (!line.empty() && (line.front() == ' ' || line.front() == '"' || line.front() == '\'')) {
            line.remove_prefix(1);
        }
        while (!line.empty() &&
               (line.back() == ' ' || line.back() == '\r' || line.back() == '"' || line.back() == '\'')) {
            line.remove_suffix(1);
        }
        return std::string{line};
    }
    return {};
}

[[nodiscard]] std::string baseName(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}
} // namespace

JavaPluginScan scanJavaPlugins(const std::string& folder) {
    JavaPluginScan scan;
    if (!wire::io::fileIO::isDirectory(folder)) {
        return scan;
    }
    std::vector<std::string> names = wire::io::fileIO::list(folder);
    std::ranges::sort(names);
    for (const std::string& name : names) {
        if (!name.ends_with(".jar")) {
            continue;
        }
        const std::string path = folder + "/" + name;
        std::optional<std::vector<std::uint8_t>> bytes = wire::io::fileIO::readBytes(path);
        if (!bytes.has_value()) {
            scan.strangers.push_back(name);
            continue;
        }
        const auto zip = wire::io::ZipReader::open(std::move(*bytes));
        if (!zip.has_value()) {
            scan.strangers.push_back(name);
            continue;
        }
        const std::string_view descriptor = zip->contains("plugin.yml") ? "plugin.yml" : "paper-plugin.yml";
        if (!zip->contains(descriptor)) {
            if (!zip->contains(kBasaltManifest)) {
                scan.strangers.push_back(name);
            }
            continue;
        }
        const auto text = zip->read(descriptor);
        JavaPluginJar jar{.path = path};
        if (text.has_value()) {
            const std::string yaml{text->begin(), text->end()};
            jar.name = valueOf(yaml, "name");
            jar.version = valueOf(yaml, "version");
        }
        if (jar.name.empty()) {
            jar.name = baseName(path);
        }
        scan.plugins.push_back(std::move(jar));
    }
    return scan;
}
} // namespace basalt::server::runtime::bukkit
