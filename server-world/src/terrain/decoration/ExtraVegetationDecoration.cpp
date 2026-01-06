#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/CactusFeature.hpp>
#include <basalt/server/world/terrain/decoration/ExtraVegetationDecoration.hpp>

#include <array>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {
namespace {

using StateSet = std::set<std::int32_t>;

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] StateSet stateIds(std::initializer_list<std::string_view> paths) {
    StateSet states;
    for (const std::string_view path : paths) {
        const registry::BlockState* found = registry::blocks::named(path);
        if (found != nullptr) {
            states.insert(found->stateId);
        }
    }
    return states;
}

const StateSet& substrateOverworld() {
    static const StateSet states = stateIds({"dirt", "coarse_dirt", "rooted_dirt", "mud", "muddy_mangrove_roots",
                                             "moss_block", "pale_moss_block", "grass_block", "podzol", "mycelium"});
    return states;
}

const StateSet& bushSupport() {
    static const StateSet states = [] {
        StateSet built = substrateOverworld();
        built.insert(stateIdOf("farmland"));
        return built;
    }();
    return states;
}

const StateSet& sugarCaneSupport() {
    static const StateSet states = [] {
        StateSet built = substrateOverworld();
        for (const std::int32_t stateId : stateIds({"sand", "red_sand", "suspicious_sand"})) {
            built.insert(stateId);
        }
        return built;
    }();
    return states;
}

const StateSet& cactusSupport() {
    static const StateSet states = stateIds({"cactus", "sand", "red_sand", "suspicious_sand"});
    return states;
}

/// parity: these features scatter up to seven blocks from their origin, so they land in a neighbour
/// often. Masking with `and 15` put those on the opposite edge of the same chunk, which is worse than
/// dropping them; the deferred writer takes them to the chunk they belong to. Coordinates are world.
[[nodiscard]] std::int32_t get(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return registry::blocks::air().stateId;
    }
    return chunk.decorationBlockStateId(x, y, z);
}

void set(Chunk& chunk, const DecorationPos& position, std::int32_t stateId) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (position.y >= bounds.minY() && position.y <= bounds.maxY()) {
        chunk.setDecorationBlockStateId(position.x, position.y, position.z, stateId);
    }
}

[[nodiscard]] bool isAir(const Chunk& chunk, const DecorationPos& position) {
    return registry::blocks::isAir(get(chunk, position.x, position.y, position.z));
}

[[nodiscard]] bool supportsSugarCaneFromSide(std::int32_t stateId) {
    static const StateSet frostedIce = stateIds({"frosted_ice"});
    return registry::blockFluids::hasWaterFluid(stateId) || frostedIce.contains(stateId);
}

[[nodiscard]] bool hasAdjacentWaterBelow(const Chunk& chunk, const DecorationPos& position) {
    return supportsSugarCaneFromSide(get(chunk, position.x + 1, position.y - 1, position.z)) ||
           supportsSugarCaneFromSide(get(chunk, position.x - 1, position.y - 1, position.z)) ||
           supportsSugarCaneFromSide(get(chunk, position.x, position.y - 1, position.z + 1)) ||
           supportsSugarCaneFromSide(get(chunk, position.x, position.y - 1, position.z - 1));
}

[[nodiscard]] bool canSugarCaneSurvive(const Chunk& chunk, const DecorationPos& position) {
    const std::int32_t below = get(chunk, position.x, position.y - 1, position.z);
    return (below == stateIdOf("sugar_cane") || sugarCaneSupport().contains(below)) &&
           hasAdjacentWaterBelow(chunk, position);
}

[[nodiscard]] bool canBushSurvive(const Chunk& chunk, const DecorationPos& position) {
    return bushSupport().contains(get(chunk, position.x, position.y - 1, position.z));
}

[[nodiscard]] bool canCactusSurvive(const Chunk& chunk, const DecorationPos& position) {
    if (!cactusSupport().contains(get(chunk, position.x, position.y - 1, position.z))) {
        return false;
    }
    const std::array<std::pair<std::int32_t, std::int32_t>, 4> sides{
        std::pair{position.x + 1, position.z}, std::pair{position.x - 1, position.z},
        std::pair{position.x, position.z + 1}, std::pair{position.x, position.z - 1}};
    for (const auto& [x, z] : sides) {
        const std::int32_t stateId = get(chunk, x, position.y, z);
        if (!registry::blocks::isAir(stateId) && heightmapMatches(HeightmapType::OceanFloor, stateId)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] DecorationPos inSquareAtHeight(const Chunk& chunk, random::RandomSource& random, HeightmapType type) {
    const std::int32_t localX = random.nextInt(16);
    const std::int32_t localZ = random.nextInt(16);
    return DecorationPos{.x = chunk.chunkX() * 16 + localX,
                         .y = chunk.heightAt(localX, localZ, type),
                         .z = chunk.chunkZ() * 16 + localZ};
}

[[nodiscard]] DecorationPos triangleOffset(const DecorationPos& origin, random::RandomSource& random,
                                           std::int32_t horizontal, std::int32_t vertical) {
    const std::int32_t dx = random.nextInt(horizontal + 1) - random.nextInt(horizontal + 1);
    const std::int32_t dy = vertical == 0 ? 0 : random.nextInt(vertical + 1) - random.nextInt(vertical + 1);
    const std::int32_t dz = random.nextInt(horizontal + 1) - random.nextInt(horizontal + 1);
    return DecorationPos{.x = origin.x + dx, .y = origin.y + dy, .z = origin.z + dz};
}

void placePumpkins(Chunk& chunk, random::RandomSource& random) {
    if (random.nextFloat() >= 1.0F / 300.0F) {
        return;
    }
    const DecorationPos origin = inSquareAtHeight(chunk, random, HeightmapType::MotionBlocking);
    for (std::int32_t attempt = 0; attempt < 96; ++attempt) {
        const DecorationPos position = triangleOffset(origin, random, 7, 3);
        if (isAir(chunk, position) && get(chunk, position.x, position.y - 1, position.z) == stateIdOf("grass_block")) {
            set(chunk, position, stateIdOf("pumpkin"));
        }
    }
}

void placeSugarCane(Chunk& chunk, random::RandomSource& random, std::optional<std::int32_t> rarity) {
    if (rarity.has_value() && random.nextFloat() >= 1.0F / static_cast<float>(*rarity)) {
        return;
    }
    const DecorationPos origin = inSquareAtHeight(chunk, random, HeightmapType::MotionBlocking);
    for (std::int32_t attempt = 0; attempt < 20; ++attempt) {
        const DecorationPos position = triangleOffset(origin, random, 4, 0);
        if (!isAir(chunk, position) || !canSugarCaneSurvive(chunk, position)) {
            continue;
        }
        const std::int32_t height = 2 + random.nextInt(random.nextInt(3) + 1);
        std::int32_t actualHeight = height;
        for (std::int32_t dy = 1; dy <= height; ++dy) {
            if (!isAir(chunk, DecorationPos{.x = position.x, .y = position.y + dy, .z = position.z})) {
                actualHeight = dy - 1;
                break;
            }
        }
        for (std::int32_t dy = 0; dy < actualHeight; ++dy) {
            set(chunk, DecorationPos{.x = position.x, .y = position.y + dy, .z = position.z}, stateIdOf("sugar_cane"));
        }
    }
}

void placeNearWaterFireflies(Chunk& chunk, random::RandomSource& random, std::int32_t outerCount,
                             bool noLeavesHeightmap) {
    const HeightmapType type =
        noLeavesHeightmap ? HeightmapType::MotionBlockingNoLeaves : HeightmapType::MotionBlocking;
    for (std::int32_t outer = 0; outer < outerCount; ++outer) {
        const DecorationPos origin = inSquareAtHeight(chunk, random, type);
        if (!isAir(chunk, origin) || !canBushSurvive(chunk, origin) || !hasAdjacentWaterBelow(chunk, origin)) {
            continue;
        }
        for (std::int32_t attempt = 0; attempt < 20; ++attempt) {
            const DecorationPos position = triangleOffset(origin, random, 4, 3);
            if (isAir(chunk, position) && canBushSurvive(chunk, position)) {
                set(chunk, position, stateIdOf("firefly_bush"));
            }
        }
    }
}

void placeGeneralFireflies(Chunk& chunk, random::RandomSource& random, std::int32_t rarity) {
    if (random.nextFloat() >= 1.0F / static_cast<float>(rarity)) {
        return;
    }
    const DecorationPos origin = inSquareAtHeight(chunk, random, HeightmapType::MotionBlocking);
    for (std::int32_t attempt = 0; attempt < 20; ++attempt) {
        const DecorationPos position = triangleOffset(origin, random, 4, 3);
        if (isAir(chunk, position) && canBushSurvive(chunk, position)) {
            set(chunk, position, stateIdOf("firefly_bush"));
        }
    }
}

void placeCactus(Chunk& chunk, random::RandomSource& random, std::int32_t rarity) {
    if (random.nextFloat() >= 1.0F / static_cast<float>(rarity)) {
        return;
    }
    const DecorationPos origin = inSquareAtHeight(chunk, random, HeightmapType::MotionBlocking);
    for (std::int32_t attempt = 0; attempt < 10; ++attempt) {
        const DecorationPos position = triangleOffset(origin, random, 7, 3);
        if (isAir(chunk, position) && canCactusSurvive(chunk, position)) {
            cactusFeature::place(chunk, random, position);
        }
    }
}

} // namespace

const BiomeVegetalDecoration& extraVegetationDecoration::pumpkinOnly() {
    static const BiomeVegetalDecoration decoration = placePumpkins;
    return decoration;
}

const BiomeVegetalDecoration& extraVegetationDecoration::standard() {
    static const BiomeVegetalDecoration decoration = [](Chunk& chunk, random::RandomSource& random) {
        placePumpkins(chunk, random);
        placeSugarCane(chunk, random, 6);
        placeNearWaterFireflies(chunk, random, 2, true);
    };
    return decoration;
}

const BiomeVegetalDecoration& extraVegetationDecoration::standardWithoutPumpkin() {
    static const BiomeVegetalDecoration decoration = [](Chunk& chunk, random::RandomSource& random) {
        placeSugarCane(chunk, random, 6);
        placeNearWaterFireflies(chunk, random, 2, true);
    };
    return decoration;
}

const BiomeVegetalDecoration& extraVegetationDecoration::fireflyNearWater() {
    static const BiomeVegetalDecoration decoration = [](Chunk& chunk, random::RandomSource& random) {
        placeNearWaterFireflies(chunk, random, 2, true);
    };
    return decoration;
}

const BiomeVegetalDecoration& extraVegetationDecoration::nearWater() { return standardWithoutPumpkin(); }

const BiomeVegetalDecoration& extraVegetationDecoration::desert() {
    static const BiomeVegetalDecoration decoration = [](Chunk& chunk, random::RandomSource& random) {
        placeSugarCane(chunk, random, std::nullopt);
        placePumpkins(chunk, random);
        placeCactus(chunk, random, 6);
    };
    return decoration;
}

const BiomeVegetalDecoration& extraVegetationDecoration::desertWithoutPumpkin() {
    static const BiomeVegetalDecoration decoration = [](Chunk& chunk, random::RandomSource& random) {
        placeSugarCane(chunk, random, std::nullopt);
        placeCactus(chunk, random, 6);
    };
    return decoration;
}

const BiomeVegetalDecoration& extraVegetationDecoration::desertWithoutPumpkinOrCactus() {
    static const BiomeVegetalDecoration decoration = [](Chunk& chunk, random::RandomSource& random) {
        placeSugarCane(chunk, random, std::nullopt);
    };
    return decoration;
}

const BiomeVegetalDecoration& extraVegetationDecoration::badlands() {
    static const BiomeVegetalDecoration decoration = [](Chunk& chunk, random::RandomSource& random) {
        placeSugarCane(chunk, random, 5);
        placePumpkins(chunk, random);
        placeCactus(chunk, random, 13);
        placeNearWaterFireflies(chunk, random, 2, true);
    };
    return decoration;
}

const BiomeVegetalDecoration& extraVegetationDecoration::badlandsWithoutPumpkin() {
    static const BiomeVegetalDecoration decoration = [](Chunk& chunk, random::RandomSource& random) {
        placeSugarCane(chunk, random, 5);
        placeCactus(chunk, random, 13);
        placeNearWaterFireflies(chunk, random, 2, true);
    };
    return decoration;
}

const BiomeVegetalDecoration& extraVegetationDecoration::badlandsWithoutPumpkinOrCactus() {
    static const BiomeVegetalDecoration decoration = [](Chunk& chunk, random::RandomSource& random) {
        placeSugarCane(chunk, random, 5);
        placeNearWaterFireflies(chunk, random, 2, true);
    };
    return decoration;
}

const BiomeVegetalDecoration& extraVegetationDecoration::swamp() {
    static const BiomeVegetalDecoration decoration = [](Chunk& chunk, random::RandomSource& random) {
        placeSugarCane(chunk, random, 3);
        placePumpkins(chunk, random);
        placeGeneralFireflies(chunk, random, 8);
        placeNearWaterFireflies(chunk, random, 3, false);
    };
    return decoration;
}

const BiomeVegetalDecoration& extraVegetationDecoration::swampWithoutPumpkin() {
    static const BiomeVegetalDecoration decoration = [](Chunk& chunk, random::RandomSource& random) {
        placeSugarCane(chunk, random, 3);
        placeGeneralFireflies(chunk, random, 8);
        placeNearWaterFireflies(chunk, random, 3, false);
    };
    return decoration;
}

const BiomeVegetalDecoration& extraVegetationDecoration::swampFireflies() {
    static const BiomeVegetalDecoration decoration = [](Chunk& chunk, random::RandomSource& random) {
        placeGeneralFireflies(chunk, random, 8);
        placeNearWaterFireflies(chunk, random, 3, false);
    };
    return decoration;
}

const BiomeVegetalDecoration& extraVegetationDecoration::mangrove() { return fireflyNearWater(); }

} // namespace basalt::server::world::terrain::decoration
