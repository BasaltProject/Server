#pragma once

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::bedrock {

/// The dye colour of a bed block state, or nothing when that state is not a bed.
///
/// Bedrock has one bed block and keeps the colour in a block entity, so a colour that is a block of
/// its own here has to be told separately over there.
[[nodiscard]] std::optional<std::uint8_t> bedColourOf(std::int32_t stateId);

} // namespace basalt::server::runtime::bedrock
