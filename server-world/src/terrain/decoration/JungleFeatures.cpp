#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/JungleFeatures.hpp>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {
namespace {

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

const std::set<std::int32_t>& melonReplaceableStateIds() {
    static const std::set<std::int32_t> ids = [] {
        std::set<std::int32_t> built;
        for (const std::string_view path :
             {"air", "short_grass", "tall_grass", "bush", "fern", "large_fern", "dandelion", "poppy"}) {
            built.insert(stateIdOf(path));
        }
        return built;
    }();
    return ids;
}

void placeMelon(Chunk& chunk, const DecorationPos& position) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (position.y < bounds.minY() || position.y > bounds.maxY() || position.y == bounds.minY()) {
        return;
    }
    const std::int32_t x = position.x & 15;
    const std::int32_t z = position.z & 15;
    const std::int32_t current = chunk.blockStateId(x, position.y, z);
    if (!melonReplaceableStateIds().contains(current) || registry::blockFluids::hasWaterFluid(current)) {
        return;
    }
    if (chunk.blockStateId(x, position.y - 1, z) != stateIdOf("grass_block")) {
        return;
    }
    chunk.setBlockStateId(x, position.y, z, stateIdOf("melon"));
}

[[nodiscard]] BiomeVegetalDecoration melonPlaced(std::int32_t rarity) {
    return [rarity](Chunk& chunk, random::RandomSource& random) {
        const DecorationPos origin{chunk.chunkX() * 16, 0, chunk.chunkZ() * 16};
        placeFrom(chunk, random, origin,
                  {rarityFilter(rarity), inSquarePlacement(), heightmapPlacement(), countPlacement(64),
                   randomOffsetPlacement(7, 3)},
                  [&chunk](const DecorationPos& position) { placeMelon(chunk, position); });
    };
}

struct VineDirection {
    std::int32_t dx;
    std::int32_t dy;
    std::int32_t dz;
    std::int32_t falseBit;
};

constexpr std::array<VineDirection, 5> kVineDirections = {
    {{0, 1, 0, 2}, {0, 0, -1, 8}, {0, 0, 1, 4}, {-1, 0, 0, 1}, {1, 0, 0, 16}}};

[[nodiscard]] std::optional<std::int32_t> stateAt(const Chunk& chunk, const DecorationPos& position) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (position.y < bounds.minY() || position.y > bounds.maxY()) {
        return std::nullopt;
    }
    return chunk.blockStateId(position.x & 15, position.y, position.z & 15);
}

bool placeVine(Chunk& chunk, const DecorationPos& origin) {
    const std::optional<std::int32_t> originState = stateAt(chunk, origin);
    if (!originState.has_value() || !registry::blocks::isAir(*originState)) {
        return false;
    }
    for (const VineDirection& direction : kVineDirections) {
        const DecorationPos neighbourPos{origin.x + direction.dx, origin.y + direction.dy, origin.z + direction.dz};
        const std::optional<std::int32_t> neighbour = stateAt(chunk, neighbourPos);
        if (!neighbour.has_value() || !heightmapMatches(HeightmapType::OceanFloor, *neighbour)) {
            continue;
        }
        chunk.setBlockStateId(origin.x & 15, origin.y, origin.z & 15, stateIdOf("vine") - direction.falseBit);
        return true;
    }
    return false;
}

} // namespace

const BiomeVegetalDecoration& jungleMelonDecoration::normal() {
    static const BiomeVegetalDecoration decoration = melonPlaced(6);
    return decoration;
}

const BiomeVegetalDecoration& jungleMelonDecoration::sparse() {
    static const BiomeVegetalDecoration decoration = melonPlaced(64);
    return decoration;
}

const BiomeVegetalDecoration& jungleVineDecoration::decoration() {
    static const BiomeVegetalDecoration built = [](Chunk& chunk, random::RandomSource& random) {
        const DecorationPos origin{chunk.chunkX() * 16, 0, chunk.chunkZ() * 16};
        placeFrom(chunk, random, origin,
                  {countPlacement(127), inSquarePlacement(), uniformHeightRangePlacement(64, 100)},
                  [&chunk](const DecorationPos& position) { placeVine(chunk, position); });
    };
    return built;
}

bool jungleVineDecoration::placeAt(Chunk& chunk, const DecorationPos& origin) { return placeVine(chunk, origin); }

} // namespace basalt::server::world::terrain::decoration
