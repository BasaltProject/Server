#include <basalt/server/plugins/PluginConfig.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <utility>

namespace basalt::server::plugins::pluginConfig {
std::expected<wire::common::toml::TomlTable, wire::plugins::PluginLoadError>
load(const std::map<std::string, std::string, std::less<>>& files) {
    wire::common::toml::TomlTable out;
    for (const auto& [relative, path] : files) {
        if (!relative.ends_with(".toml")) {
            continue;
        }
        const std::optional<std::string> text = wire::io::fileIO::readText(path);
        if (!text.has_value()) {
            continue;
        }
        std::expected<wire::common::toml::TomlTable, wire::common::toml::TomlError> parsed =
            wire::common::toml::parse(*text);
        if (!parsed.has_value()) {
            return std::unexpected(wire::plugins::PluginLoadError{
                .message =
                    path + " is malformed: " + std::string{wire::common::toml::tomlErrorMessage(parsed.error())}});
        }
        if (relative == kRootFile) {
            for (const std::string& key : parsed->keys()) {
                const wire::common::toml::TomlValue* value = parsed->find(key);
                if (value != nullptr) {
                    out.set(key, *value);
                }
            }
            continue;
        }
        std::string name = relative;
        const std::size_t slash = name.rfind('/');
        if (slash != std::string::npos) {
            name = name.substr(slash + 1);
        }
        name.resize(name.size() - std::string_view{".toml"}.size());
        out.set(name, wire::common::toml::TomlValue{std::move(*parsed)});
    }
    return out;
}
} // namespace basalt::server::plugins::pluginConfig
