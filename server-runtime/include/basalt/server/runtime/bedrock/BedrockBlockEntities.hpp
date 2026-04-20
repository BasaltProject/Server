#pragma once

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::bedrock {

[[nodiscard]] std::optional<std::uint8_t> bedColourOf(std::int32_t stateId);

[[nodiscard]] std::optional<std::uint8_t> bannerColourOf(std::int32_t stateId);

} // namespace basalt::server::runtime::bedrock
