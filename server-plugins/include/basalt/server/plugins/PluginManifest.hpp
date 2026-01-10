#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

/// A manifest that says something the loader cannot work with, named so the author can fix it.
struct ManifestError {
    std::string message;
};

/// What a plugin may reach that the operator has to say yes to.
///
/// The rest of a plugin's capabilities stay inside its own sandbox: its folder, its own rows in the
/// database. These three leave it. Watching packets is reading everyone's chat, a channel talks to
/// whatever is in front of the client, and running a command as the console is being the console.
enum class PluginPermission : std::uint8_t { Packets, Network, Commands };

[[nodiscard]] std::string_view permissionName(PluginPermission permission);

[[nodiscard]] std::optional<PluginPermission> permissionOf(std::string_view name);

/// What a plugin may reach beyond its own package. Nothing is granted that is not asked for here.
struct PluginCapabilities {
    bool storage = false;
    bool playersData = false;
    bool commands = false;
    std::vector<std::string> files;
    std::vector<std::string> network;
    /// Whether it may see packets. Never granted by asking nicely at runtime: somebody watching
    /// packets sees everybody's chat, so it is written in the manifest where an operator reads it
    /// before installing anything.
    bool packets = false;

    /// The ones an operator has to grant, of those this plugin asked for.
    [[nodiscard]] std::set<PluginPermission> asks() const;
};

/// What a plugin lets the others reach: its public surface, and nothing else is.
///
/// Declared by the owner and only by the owner. There is no operator setting that forces the exports
/// of somebody else's plugin, and that is not prudishness: whoever ate the breakage after an update
/// would be the one who wrote neither of the two.
struct PluginExports {
    /// Files of `src/` another plugin may require, without the extension. `["*"]` opens all of it.
    std::vector<std::string> modules;
    /// Names this plugin may offer through `service(name, table)`.
    std::vector<std::string> services;

    [[nodiscard]] bool opensEverything() const;

    [[nodiscard]] bool exportsModule(std::string_view module) const;
};

/// One dependency, and how much of it will do.
struct PluginDependency {
    std::string id;
    /// `*`, `1.2.3` or `>=1.2`. Nothing fancier until a plugin needs it.
    std::string requirement;
    bool hard = true;

    [[nodiscard]] bool accepts(std::string_view version) const;
};

/// Compares two dotted versions numerically, treating a missing part as zero.
[[nodiscard]] std::int32_t compareVersions(std::string_view left, std::string_view right);

/// `plugin.toml`, which is the only file the loader reads before deciding whether to load anything.
///
/// It lives at the root of the package rather than under `resources/` for that reason: knowing
/// whether a package can be loaded should not mean unpacking it.
struct PluginManifest {
    /// The API major this server speaks. A package asking for a different one does not load.
    static constexpr std::int32_t kApi = 1;
    static constexpr std::string_view kFileName = "plugin.toml";

    std::string id;
    std::string name;
    std::string version;
    std::vector<std::string> authors;
    /// The major of the plugin API this was written against.
    std::int32_t api = 0;
    /// Path inside the package, `src/main.lua` unless it says otherwise.
    std::string main;
    std::vector<PluginDependency> dependencies;
    PluginCapabilities capabilities;
    PluginExports exports;
    /// Paths under `resources/` that stay inside the package instead of being copied into the
    /// plugin's folder: templates, seed data, anything the plugin reads and the operator has no
    /// business editing.
    std::vector<std::string> privateResources;

    [[nodiscard]] std::vector<PluginDependency> hardDependencies() const;

    [[nodiscard]] static std::expected<PluginManifest, ManifestError> parse(std::string_view text);
};

} // namespace basalt::server::plugins
