#pragma once

#include <basalt/server/plugins/PluginPackets.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace basalt::server::runtime::plugins::packetNames {
struct Row {
    server::plugins::PacketPhase phase;
    bool serverbound;
    std::int32_t id;
    std::string_view name;
};

[[nodiscard]] const std::span<const Row> rows();

[[nodiscard]] const std::span<const Row> aliases();
} // namespace basalt::server::runtime::plugins::packetNames
