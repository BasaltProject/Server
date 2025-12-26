#pragma once

#include <basalt/wire/protocol/ProtocolState.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::plugins::packetNames {

/// What each packet is called, so a plugin can name one without knowing its number.
///
/// The protocol's own names with the direction in front, not a dictionary invented for plugins. The
/// direction comes from what this server does with each one: dispatched on the way in is
/// serverbound, sent is clientbound.

/// `clientbound.set_title_text`, or the number in hex when this build has no name for it.
[[nodiscard]] std::string of(wire::protocol::ProtocolState state, bool serverbound, std::int32_t id);

/// The state and number behind a name, for sending one.
[[nodiscard]] std::optional<std::pair<wire::protocol::ProtocolState, std::int32_t>> idOf(std::string_view name);

[[nodiscard]] bool isKnown(std::string_view name);

/// Every name, for the error message when a plugin misspells one.
[[nodiscard]] std::vector<std::string> all();

} // namespace basalt::server::runtime::plugins::packetNames
