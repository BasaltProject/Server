#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/bukkit/BukkitRuntimeFiles.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>

#ifdef BASALT_WITH_BUKKIT
#include <basalt/server/bukkit/BukkitHost.hpp>
#endif

namespace basalt::server::runtime {
namespace {
[[nodiscard]] std::string lowered(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}
} // namespace

void BasaltServer::startBukkit() {
    const config::BukkitConfig& settings = options_.bukkit;
    if (!config_->pluginsEnabled) {
        return;
    }
    const std::string folder = plugins_->services().paths().packages();
    bukkit::JavaPluginScan scan = bukkit::scanJavaPlugins(folder);
    if (settings.mode != config::BukkitConfig::Mode::Off) {
        for (const std::string& stranger : scan.strangers) {
            logger_.warn([&] {
                return std::format("{}/{} is not a plugin (no plugin.yml), so it is ignored.", folder, stranger);
            });
        }
    }
    std::error_code ignored;
    if (std::filesystem::is_directory("plugins-bukkit", ignored) &&
        !bukkit::scanJavaPlugins("plugins-bukkit").plugins.empty()) {
        logger_.warn([&] { return std::format("plugins-bukkit/ is no longer read: move its jars into {}/.", folder); });
    }
    if (scan.plugins.empty()) {
        return;
    }
    javaPlugins_ = std::move(scan.plugins);
    const std::size_t count = javaPlugins_.size();
    if (settings.mode == config::BukkitConfig::Mode::Off) {
        logger_.info([&] {
            return std::format("{} Bukkit plugin(s) in {}/ are skipped: bukkit.toml sets enabled = false.", count,
                               folder);
        });
        return;
    }
#ifdef BASALT_WITH_BUKKIT
    const wire::common::Logger bukkitLogger{"Bukkit"};
    const std::expected<bukkit::BukkitRuntimeFiles, std::string> files = bukkit::locateRuntime(settings, bukkitLogger);
    if (!files.has_value()) {
        logger_.error(
            [&] { return std::format("{} Bukkit plugin(s) in {}/ cannot load: {}.", count, folder, files.error()); });
        return;
    }
    server::bukkit::BukkitOptions options{.libjvm = files->libjvm,
                                          .bootClasspath = files->bootClasspath,
                                          .runtimeJar = files->runtimeJar,
                                          .libraries = files->libraries,
                                          .pluginsFolder = folder};
    std::expected<std::unique_ptr<server::bukkit::BukkitHost>, std::string> host =
        server::bukkit::BukkitHost::start(options, *plugins_);
    if (!host.has_value()) {
        logger_.error([&host] { return "Bukkit compatibility could not start: " + host.error(); });
        return;
    }
    for (const server::bukkit::BukkitHost::PluginState& state : (*host)->plugins()) {
        for (bukkit::JavaPluginJar& jar : javaPlugins_) {
            if (lowered(jar.name) == lowered(state.name)) {
                jar.loaded = state.enabled;
                jar.version = state.version;
            }
        }
    }
    bukkit_ = std::shared_ptr<server::bukkit::BukkitHost>(std::move(*host));
#else
    logger_.warn([&] {
        return std::format(
            "{} Bukkit plugin(s) in {}/ cannot load: this build has no Bukkit support. Install a JDK and "
            "rebuild, or use a release that includes it.",
            count, folder);
    });
#endif
}
} // namespace basalt::server::runtime
