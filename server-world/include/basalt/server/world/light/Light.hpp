#pragma once

#include <basalt/server/world/light/LightWindow.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace basalt::server::world::light {

/// One nibble level per block of a section, in `(y * 16 + z) * 16 + x` order.
using SectionLight = std::vector<std::int32_t>;

/// Sky light for one chunk, column by column and then spread sideways.
///
/// parity: `SkyLightEngine`. Light falls straight down a column at full strength until something
/// stops it, and from there it spreads in every direction losing one level per block, plus whatever
/// the block itself absorbs.
///
/// Two deliberate limits, both visible only at a chunk border: the spread reads one block into the
/// neighbouring chunks rather than lighting the 3x3 the way vanilla does, and a neighbour that is
/// not loaded counts as open sky. Both err bright. The bug they replaced erred black over a whole
/// chunk, which is what a hill or an End pillar used to do to everything under it.
namespace skyLight {

inline constexpr std::int32_t kMax = 15;

/// How much light a block takes out of what passes through it.
///
/// parity: `BlockBehaviour.getLightDampening`, whose answer comes with the block's own shape:
/// fifteen for one that fills its cube, one for a shape that leaves a gap but holds a fluid, zero
/// for the rest. A slab or a trapdoor therefore stops nothing at all.
[[nodiscard]] std::int32_t opacity(std::int32_t stateId);

/// The same answers as one table, for the bulk fill that reads it a few hundred thousand times.
[[nodiscard]] std::span<const std::int32_t> opacityTable();

/// The sky light of the middle chunk of `window`, one array per section.
///
/// Only the band with terrain in it is lit: above the tallest block everything is open sky, and more
/// than fifteen blocks below the lowest surface no sky light can reach, since it loses a level per
/// block on the way down.
[[nodiscard]] std::vector<SectionLight> compute(const LightWindow& window);

} // namespace skyLight

/// Light from the blocks themselves: lava, glowstone, a torch in a cave.
///
/// parity: `BlockLightEngine`, seeded from each block's own light level instead of from the sky. The
/// spread is the same one the sky uses, so the two stay in step.
///
/// The emission table is written here rather than generated: the official `blocks.json` report does
/// not carry light levels, and a block that emits none needs no entry.
namespace blockLight {

/// parity: `BlockBehaviour.Properties.lightLevel` of the blocks that have one.
[[nodiscard]] std::int32_t emission(std::int32_t stateId);

/// The block light of the middle chunk of `window`, or nothing when nothing in reach burns.
///
/// The band is the sections that hold something burning, grown by one on each side: fifteen levels
/// reach less than a section away, so nothing outside it can be lit.
[[nodiscard]] std::optional<std::vector<SectionLight>> compute(const LightWindow& window);

} // namespace blockLight

} // namespace basalt::server::world::light
