#pragma once

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `AxeItem.evaluateNewBlockState`.
namespace waxing {

/// The waxed form of this state, keeping the shape it had. parity: `HoneycombItem.getWaxed`.
[[nodiscard]] std::optional<std::int32_t> waxed(std::int32_t stateId);

enum class Cut : std::uint8_t { Stripped, Scraped, Unwaxed };

struct Scrape {
    std::int32_t stateId = 0;
    Cut cut = Cut::Stripped;
};

[[nodiscard]] std::optional<Scrape> scraped(std::int32_t stateId);

} // namespace waxing

} // namespace basalt::server::runtime::world
