#include <basalt/server/runtime/config/BukkitConfig.hpp>
#include <basalt/server/runtime/config/ConfigDefaults.hpp>

namespace basalt::server::runtime::config {
namespace {
std::expected<BukkitConfig::Mode, std::string> modeOf(const wire::common::toml::TomlTable& root,
                                                      const std::string& file) {
    if (const std::optional<bool> flag = root.boolean("enabled"); flag.has_value()) {
        return *flag ? BukkitConfig::Mode::On : BukkitConfig::Mode::Off;
    }
    const std::optional<std::string_view> text = root.string("enabled");
    if (!text.has_value() || *text == "auto") {
        return BukkitConfig::Mode::Auto;
    }
    if (*text == "true" || *text == "on") {
        return BukkitConfig::Mode::On;
    }
    if (*text == "false" || *text == "off") {
        return BukkitConfig::Mode::Off;
    }
    return std::unexpected(file + ": enabled must be \"auto\", true or false");
}
} // namespace

std::expected<BukkitConfig, std::string> BukkitConfig::load(const ConfigDirectory& directory) {
    const std::expected<wire::common::toml::TomlTable, std::string> root =
        directory.load(kFileName, kBukkitDefaultToml);
    if (!root.has_value()) {
        return std::unexpected(root.error());
    }
    BukkitConfig config;
    const std::expected<Mode, std::string> mode = modeOf(*root, directory.pathOf(kFileName));
    if (!mode.has_value()) {
        return std::unexpected(mode.error());
    }
    config.mode = *mode;
    config.download = root->boolean("download").value_or(true);
    config.runtime = std::string{root->string("runtime").value_or("")};
    config.libjvm = std::string{root->string("libjvm").value_or("")};
    config.bootClasspath = std::string{root->string("boot-classpath").value_or("")};
    config.runtimeJar = std::string{root->string("runtime-jar").value_or("")};
    config.libraries = std::string{root->string("libraries").value_or("")};
    return config;
}
} // namespace basalt::server::runtime::config
