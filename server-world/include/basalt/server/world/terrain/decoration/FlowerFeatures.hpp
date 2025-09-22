#pragma once

#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// Closed-eyeblossom placed features used only by the pale garden. Both configured features request
/// a scheduled tick, so each accepted flower schedules its own one-tick block update.
namespace paleGardenDecoration {

/// `pale_garden_flowers`: rarity 1/8, then a 96-candidate patch below leaf cover.
[[nodiscard]] const BiomeVegetalDecoration& flowerPatch();

/// `flower_pale_garden`: a separate rarity-1/32 single placement.
[[nodiscard]] const BiomeVegetalDecoration& rareFlower();

/// The patch at one origin, which is what `/place feature` runs for `pale_garden_flowers`.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

/// The single flower `flower_pale_garden` places, with no patch around it.
void placeSingleAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace paleGardenDecoration

/// The cherry grove's pink petals. Vanilla's weighted provider keeps all 16 states: amount 1 to 4
/// crossed with four horizontal facings.
///
/// Gap: the biome's `trees_cherry` tail is not here, because the tree features are not ported yet.
/// What this covers is `flower_cherry` and nothing else.
namespace cherryGroveDecoration {

/// The petal patch as the biome runs it, noise-counted over the chunk.
[[nodiscard]] const BiomeVegetalDecoration& flowersOnly();

/// The petal patch at one origin, which is what `/place feature` runs for `flower_cherry`.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

/// One of the 16 pink petal states, by the weighted provider's choice index.
[[nodiscard]] std::int32_t pinkPetalsStateId(std::int32_t choice);

} // namespace cherryGroveDecoration

} // namespace basalt::server::world::terrain::decoration
