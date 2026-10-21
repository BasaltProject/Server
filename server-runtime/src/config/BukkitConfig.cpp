#include <basalt/server/runtime/config/BukkitConfig.hpp>
#include <basalt/server/runtime/config/ConfigDefaults.hpp>

namespace basalt::server::runtime::config {
std::expected<BukkitConfig, std::string> BukkitConfig::load(const ConfigDirectory& directory) {
    const std::expected<wire::common::toml::TomlTable, std::string> root =
        directory.load(kFileName, kBukkitDefaultToml);
    if (!root.has_value()) {
        return std::unexpected(root.error());
    }
    BukkitConfig config;
    config.enabled = root->boolean("enabled").value_or(false);
    config.libjvm = std::string{root->string("libjvm").value_or("")};
    config.bootClasspath = std::string{root->string("boot-classpath").value_or("")};
    config.runtimeJar = std::string{root->string("runtime-jar").value_or("")};
    config.libraries = std::string{root->string("libraries").value_or("")};
    config.plugins = std::string{root->string("plugins").value_or("plugins-bukkit")};
    if (config.enabled && (config.libjvm.empty() || config.bootClasspath.empty() || config.runtimeJar.empty())) {
        return std::unexpected(directory.pathOf(kFileName) +
                               ": libjvm, boot-classpath and runtime-jar must be set when enabled is true");
    }
    return config;
}
} // namespace basalt::server::runtime::config
