#pragma once

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// What a honeycomb and an axe turn a block into. parity: `AxeItem.evaluateNewBlockState` tries
/// strip, scrape one step of oxidation, unwax, in that order. Vanilla's maps are lookups by name
/// here, which comes to the same set: each of those blocks is named after the one it came from.
namespace waxing {

/// The waxed form of this state, keeping the shape it had. parity: `HoneycombItem.getWaxed`.
[[nodiscard]] std::optional<std::int32_t> waxed(std::int32_t stateId);

/// Which of the axe's three things happened, which is what decides the sound and the particles.
enum class Cut : std::uint8_t { Stripped, Scraped, Unwaxed };

struct Scrape {
    std::int32_t stateId = 0;
    Cut cut = Cut::Stripped;
};

/// What an axe leaves: a stripped log, a copper one step fresher, or the same block unwaxed.
[[nodiscard]] std::optional<Scrape> scraped(std::int32_t stateId);

} // namespace waxing

} // namespace basalt::server::runtime::world
