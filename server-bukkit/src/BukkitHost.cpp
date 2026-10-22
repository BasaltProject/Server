#include "BridgeInternal.hpp"

#include <algorithm>
#include <filesystem>

namespace basalt::server::bukkit {
namespace {
[[nodiscard]] std::vector<std::string> classpathOf(const BukkitOptions& options) {
    std::vector<std::string> path{std::filesystem::absolute(options.runtimeJar).string()};
    for (const std::string& jar : options.libraries) {
        path.push_back(std::filesystem::absolute(jar).string());
    }
    return path;
}
} // namespace

BukkitHost::BukkitHost(Key) {}

BukkitHost::~BukkitHost() { stop(); }

std::expected<std::unique_ptr<BukkitHost>, std::string> BukkitHost::start(const BukkitOptions& options,
                                                                          plugins::PluginHost& plugins) {
    for (const std::string* path : {&options.libjvm, &options.bootClasspath, &options.runtimeJar}) {
        if (path->empty() || !std::filesystem::exists(*path)) {
            return std::unexpected("'" + *path + "' does not exist");
        }
    }
    ShimOptions shimOptions{.libjvm = std::filesystem::absolute(options.libjvm).string(),
                            .bootClasspath = std::filesystem::absolute(options.bootClasspath).string(),
                            .classpath = classpathOf(options)};
    std::expected<std::unique_ptr<ShimHost>, std::string> shim = ShimHost::create(shimOptions);
    if (!shim.has_value()) {
        return std::unexpected(shim.error());
    }
    auto host = std::make_unique<BukkitHost>(Key{});
    host->shim_ = std::move(*shim);
    host->bridge_ = std::make_unique<BukkitBridge>(*host->shim_, plugins, options);
    if (const std::expected<void, std::string> booted = host->bridge_->boot(); !booted.has_value()) {
        return std::unexpected(booted.error());
    }
    return host;
}

std::vector<BukkitHost::PluginState> BukkitHost::plugins() const { return bridge_->javaPlugins(); }

void BukkitHost::stop() {
    if (stopped_ || bridge_ == nullptr) {
        return;
    }
    stopped_ = true;
    shim_->beginShutdown();
    if (bridge_->shutdown()) {
        shim_->destroy();
    } else {
        ShimHost::markLeftRunning();
    }
}
} // namespace basalt::server::bukkit
