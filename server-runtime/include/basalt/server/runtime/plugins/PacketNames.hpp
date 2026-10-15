#pragma once

#include <basalt/wire/protocol/ProtocolState.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::plugins::packetNames {

[[nodiscard]] std::string of(wire::protocol::ProtocolState state, bool serverbound, std::int32_t id);

[[nodiscard]] std::optional<std::pair<wire::protocol::ProtocolState, std::int32_t>> idOf(std::string_view name);

[[nodiscard]] bool isKnown(std::string_view name);

[[nodiscard]] std::vector<std::string> all();

} // namespace basalt::server::runtime::plugins::packetNames
