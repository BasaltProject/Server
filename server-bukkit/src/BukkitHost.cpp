#include "BridgeInternal.hpp"

#include <algorithm>
#include <filesystem>

namespace basalt::server::bukkit {
namespace {
[[nodiscard]] std::vector<std::string> classpathOf(const BukkitOptions& options) {
    std::vector<std::string> path{options.runtimeJar};
    std::vector<std::string> jars;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(options.libraries, error)) {
        if (entry.path().extension() == ".jar") {
            jars.push_back(std::filesystem::absolute(entry.path()).string());
        }
    }
    std::ranges::sort(jars);
    path.insert(path.end(), jars.begin(), jars.end());
    return path;
}
} // namespace

BukkitHost::BukkitHost(Key) {}

BukkitHost::~BukkitHost() { stop(); }

std::expected<std::unique_ptr<BukkitHost>, std::string> BukkitHost::start(const BukkitOptions& options,
                                                                          plugins::PluginHost& plugins) {
    for (const std::string* path : {&options.libjvm, &options.bootClasspath, &options.runtimeJar}) {
        if (path->empty() || !std::filesystem::exists(*path)) {
            return std::unexpected("bukkit.toml points at '" + *path + "', which does not exist");
        }
    }
    ShimOptions shimOptions{.libjvm = std::filesystem::absolute(options.libjvm).string(),
                            .bootClasspath = std::filesystem::absolute(options.bootClasspath).string(),
                            .classpath = classpathOf(options)};
    shimOptions.classpath.front() = std::filesystem::absolute(options.runtimeJar).string();
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

void BukkitHost::stop() {
    if (stopped_ || bridge_ == nullptr) {
        return;
    }
    stopped_ = true;
    bridge_->shutdown();
}
} // namespace basalt::server::bukkit
