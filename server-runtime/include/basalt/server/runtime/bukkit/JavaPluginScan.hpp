#pragma once

#include <string>
#include <vector>

namespace basalt::server::runtime::bukkit {
struct JavaPluginJar {
    std::string path;
    std::string name;
    std::string version;
    bool loaded = false;
};

struct JavaPluginScan {
    std::vector<JavaPluginJar> plugins;
    std::vector<std::string> strangers;
};

/// Finds the Bukkit and Paper plugins in a folder by looking inside each jar for a plugin.yml, never by its name.
[[nodiscard]] JavaPluginScan scanJavaPlugins(const std::string& folder);
} // namespace basalt::server::runtime::bukkit
