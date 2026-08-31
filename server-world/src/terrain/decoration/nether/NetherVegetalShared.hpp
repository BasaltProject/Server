#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/NetherBiomeSource.hpp>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::decoration::nether {

/// The reads and writes every Nether vegetal feature shares, plus the two configuration shapes the
/// forest and the fungi are parameterised by. Private to this directory: the step's own header is
/// the public door.
namespace shared {

using PathSet = std::set<std::string, std::less<>>;

struct Pos {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

[[nodiscard]] std::int32_t stateId(std::string_view path);

/// The terrain read: outside this chunk, and outside its vertical bounds, there is nothing to see.
[[nodiscard]] std::optional<std::int32_t> terrainStateAt(const Chunk& chunk, std::int32_t x, std::int32_t y,
                                                         std::int32_t z);

/// The decoration read, which crosses the chunk edge into whatever the phase reader supplies.
[[nodiscard]] std::optional<std::int32_t> stateAt(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] std::string blockPath(std::int32_t stateId);

[[nodiscard]] std::string blockPathAt(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] bool isAir(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z);

void setBlock(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

[[nodiscard]] std::int32_t triangle(random::RandomSource& random, std::int32_t range);

/// Candidate origins on every ground layer of a chunk, which the fungi and the forest both scatter
/// over.
[[nodiscard]] std::vector<Pos> positionsOnEveryLayer(const Chunk& chunk, random::RandomSource& random,
                                                     std::int32_t count);

/// parity: the four `nether_forest_vegetation` configurations, which differ in their weighted state
/// provider as well as their layer count.
enum class ForestVegetation : std::uint8_t { Warped, Sprouts, Crimson };

[[nodiscard]] std::int32_t forestLayerCount(ForestVegetation config);
[[nodiscard]] std::string_view forestBiome(ForestVegetation config);
[[nodiscard]] std::string_view sampleForestState(ForestVegetation config, random::RandomSource& random);

/// parity: `HugeFungusConfiguration`'s two real instances.
struct HugeFungus {
    std::string_view biome;
    std::string_view validBase;
    std::string_view stem;
    std::string_view hat;
    std::string_view decor;
};

[[nodiscard]] const HugeFungus& crimsonFungus();
[[nodiscard]] const HugeFungus& warpedFungus();

void placeVegetationAt(Chunk& chunk, random::RandomSource& random, const Pos& origin, ForestVegetation config);
void placeForestVegetation(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource,
                           ForestVegetation config);

void placeNormalMushroom(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource,
                         std::int32_t rarity, std::string_view mushroom);

void placeLavaSprings(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource);
void placeLavaSpring(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z);

void placeTwistingVines(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource);
void placeTwistingVinesFeature(Chunk& chunk, random::RandomSource& random, const Pos& origin);

void placeWeepingVines(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource);
void placeWeepingVinesFeature(Chunk& chunk, random::RandomSource& random, const Pos& origin);
void placeWeepingColumn(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t startY, std::int32_t z,
                        std::int32_t totalHeight, std::int32_t minAge);

void placeHugeFungi(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource,
                    const HugeFungus& config);
void placeHugeFungus(Chunk& chunk, random::RandomSource& random, const Pos& origin, const HugeFungus& config);

} // namespace shared

} // namespace basalt::server::world::terrain::decoration::nether
