#pragma once

#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/remote/RemoteHost.hpp>
#include <basalt/server/runtime/TickLoop.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>

#include <chrono>
#include <string>

namespace basalt::server::runtime::remoteHost {

struct Sources {
    command::CommandDispatcher* dispatcher = nullptr;
    server::plugins::PluginHost* plugins = nullptr;
    const TickLoop* tick = nullptr;
    const player::PlayerRegistry* players = nullptr;
    std::int32_t maxPlayers = 0;
    std::chrono::steady_clock::time_point startedAt;
};

[[nodiscard]] std::unique_ptr<server::remote::RemoteHost> create(Sources sources);

} // namespace basalt::server::runtime::remoteHost
