#include <basalt/server/runtime/BasaltServer.hpp>

#ifdef BASALT_WITH_BUKKIT
#include <basalt/server/bukkit/BukkitHost.hpp>
#endif

namespace basalt::server::runtime {
void BasaltServer::startBukkit() {
    if (!options_.bukkit.enabled) {
        return;
    }
#ifdef BASALT_WITH_BUKKIT
    const config::BukkitConfig& settings = options_.bukkit;
    bukkit::BukkitOptions options{.libjvm = settings.libjvm,
                                  .bootClasspath = settings.bootClasspath,
                                  .runtimeJar = settings.runtimeJar,
                                  .libraries = settings.libraries,
                                  .pluginsFolder = settings.plugins};
    std::expected<std::unique_ptr<bukkit::BukkitHost>, std::string> host =
        bukkit::BukkitHost::start(options, *plugins_);
    if (!host.has_value()) {
        logger_.error([&host] { return "Bukkit compatibility could not start: " + host.error(); });
        return;
    }
    bukkit_ = std::shared_ptr<bukkit::BukkitHost>(std::move(*host));
#else
    logger_.warn([] { return std::string{"bukkit.toml turns Bukkit compatibility on, but this build has none."}; });
#endif
}
} // namespace basalt::server::runtime
