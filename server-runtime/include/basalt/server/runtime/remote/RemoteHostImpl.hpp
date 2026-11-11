#pragma once

#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/remote/ManagementNotifier.hpp>
#include <basalt/server/remote/RemoteHost.hpp>
#include <basalt/server/runtime/TickLoop.hpp>
#include <basalt/server/runtime/bukkit/JavaPluginScan.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/plugins/ServerSettings.hpp>

#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace basalt::server::runtime::remoteHost {

struct Sources {
    command::CommandDispatcher* dispatcher = nullptr;
    server::plugins::PluginHost* plugins = nullptr;
    const TickLoop* tick = nullptr;
    const player::PlayerRegistry* players = nullptr;
    std::int32_t maxPlayers = 0;
    std::function<std::vector<bukkit::JavaPluginJar>()> javaPlugins;
    std::chrono::steady_clock::time_point startedAt;
    server::remote::ManagementBackend* management = nullptr;
};

struct ManagementSources {
    command::CommandDispatcher* dispatcher = nullptr;
    const player::PlayerRegistry* players = nullptr;
    plugins::ServerSettings* settings = nullptr;
    server::remote::ManagementNotifier* notifier = nullptr;
    std::function<void()> saveAll;
    std::function<void()> stop;
    std::function<bool()> ready;
};

[[nodiscard]] std::unique_ptr<server::remote::ManagementBackend> createManagement(ManagementSources sources);

/// Routes whitelist, ban and operator changes, from any source, to the notifier.
void observeAccess(ManagementSources sources);

[[nodiscard]] std::unique_ptr<server::remote::RemoteHost> create(Sources sources);

} // namespace basalt::server::runtime::remoteHost
