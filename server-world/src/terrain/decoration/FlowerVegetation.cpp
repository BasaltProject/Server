#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/FlowerVegetation.hpp>

#include <array>
#include <optional>
#include <stdexcept>
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

[[nodiscard]] DecorationPos chunkOrigin(const Chunk& chunk) {
    return DecorationPos{chunk.chunkX() * 16, 0, chunk.chunkZ() * 16};
}

struct FlowerState {
    std::string_view path;
    bool doublePlant;
};

// parity: SimpleRandomSelectorFeature preserves this configured-feature insertion order.
constexpr std::array<FlowerState, 4> kFlowerStates = {
    {{"lilac", true}, {"rose_bush", true}, {"peony", true}, {"lily_of_the_valley", false}}};

void placeSelectedFlowerPatch(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    const FlowerState& selected =
        kFlowerStates[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kFlowerStates.size())))];
    const std::int32_t stateId = stateIdOf(selected.path);
    const SimpleBlockFeature feature{[stateId](random::RandomSource&, const DecorationPos&) { return stateId; },
                                     selected.doublePlant};
    placeFrom(chunk, random, origin, {countPlacement(96), randomOffsetPlacement(7, 3), airTargetFilter()},
              [&chunk, &random, &feature](const DecorationPos& position) { feature.place(chunk, random, position); });
}

[[nodiscard]] BiomeVegetalDecoration flowerVariant(IntProvider outerCount) {
    return [outerCount = std::move(outerCount)](Chunk& chunk, random::RandomSource& random) {
        placeFrom(chunk, random, chunkOrigin(chunk),
                  {rarityFilter(7), inSquarePlacement(), heightmapPlacement(), countPlacement(outerCount)},
                  [&chunk, &random](const DecorationPos& origin) { placeSelectedFlowerPatch(chunk, random, origin); });
    };
}

void placeMushroomPatch(Chunk& chunk, random::RandomSource& random, std::string_view path,
                        std::optional<std::int32_t> rarity, std::int32_t outerCount = 1) {
    const std::int32_t stateId = stateIdOf(path);
    const SimpleBlockFeature feature{[stateId](random::RandomSource&, const DecorationPos&) { return stateId; }};
    std::vector<PlacementModifier> modifiers;
    if (outerCount != 1) {
        modifiers.push_back(countPlacement(outerCount));
    }
    if (rarity.has_value()) {
        modifiers.push_back(rarityFilter(*rarity));
    }
    modifiers.push_back(inSquarePlacement());
    modifiers.push_back(heightmapPlacement());
    modifiers.push_back(countPlacement(96));
    modifiers.push_back(randomOffsetPlacement(7, 3));
    modifiers.push_back(airTargetFilter());
    runFromChunkCorner(chunk, random, modifiers, feature);
}

[[nodiscard]] PlacementModifier airNotOnPodzol() {
    return filter([](const PlacementContext& context, random::RandomSource&, const DecorationPos& position) {
        const ChunkHeight bounds = context.chunk().verticalBounds();
        if (position.y <= bounds.minY() || position.y > bounds.maxY()) {
            return false;
        }
        const std::int32_t target = context.chunk().decorationBlockStateId(position.x, position.y, position.z);
        const std::int32_t below = context.chunk().decorationBlockStateId(position.x, position.y - 1, position.z);
        const wire::common::Identifier* belowBlock = registry::blockStateDefinitions::blockOf(below);
        return registry::blocks::isAir(target) && (belowBlock == nullptr || belowBlock->path() != "podzol");
    });
}

} // namespace

const BiomeVegetalDecoration& forestFlowerDecoration::normal() {
    static const BiomeVegetalDecoration decoration =
        flowerVariant(IntProvider::clamped(IntProvider::uniform(-3, 1), 0, 1));
    return decoration;
}

const BiomeVegetalDecoration& forestFlowerDecoration::flowerForest() {
    static const BiomeVegetalDecoration decoration =
        flowerVariant(IntProvider::clamped(IntProvider::uniform(-1, 3), 0, 3));
    return decoration;
}

const std::set<std::string, std::less<>>& forestFlowerDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = {"forest_flowers", "flower_flower_forest",
                                                           "flower_forest_flowers"};
    return ids;
}

void forestFlowerDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    placeSelectedFlowerPatch(chunk, random, origin);
}

const BiomeVegetalDecoration& mushroomDecoration::normal() {
    static const BiomeVegetalDecoration decoration = [](Chunk& chunk, random::RandomSource& random) {
        placeMushroomPatch(chunk, random, "brown_mushroom", 256);
        placeMushroomPatch(chunk, random, "red_mushroom", 512);
    };
    return decoration;
}

const BiomeVegetalDecoration& mushroomDecoration::taiga() {
    static const BiomeVegetalDecoration decoration = [](Chunk& chunk, random::RandomSource& random) {
        placeMushroomPatch(chunk, random, "brown_mushroom", 4);
        placeMushroomPatch(chunk, random, "red_mushroom", 256);
    };
    return decoration;
}

const BiomeVegetalDecoration& mushroomDecoration::oldGrowth() {
    static const BiomeVegetalDecoration decoration = [](Chunk& chunk, random::RandomSource& random) {
        placeMushroomPatch(chunk, random, "brown_mushroom", 4, 3);
        placeMushroomPatch(chunk, random, "red_mushroom", 171);
    };
    return decoration;
}

const BiomeVegetalDecoration& mushroomDecoration::swamp() {
    static const BiomeVegetalDecoration decoration = [](Chunk& chunk, random::RandomSource& random) {
        placeMushroomPatch(chunk, random, "brown_mushroom", std::nullopt, 2);
        placeMushroomPatch(chunk, random, "red_mushroom", 64);
    };
    return decoration;
}

void jungleGrassDecoration::placePatchAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    const SimpleBlockFeature feature{[](random::RandomSource& r, const DecorationPos&) {
        return r.nextInt(4) < 3 ? stateIdOf("short_grass") : stateIdOf("fern");
    }};
    placeFrom(chunk, random, origin, {countPlacement(32), randomOffsetPlacement(7, 3), airNotOnPodzol()},
              [&chunk, &random, &feature](const DecorationPos& position) { feature.place(chunk, random, position); });
}

const BiomeVegetalDecoration& jungleGrassDecoration::decoration() {
    static const BiomeVegetalDecoration built = [](Chunk& chunk, random::RandomSource& random) {
        placeFrom(chunk, random, chunkOrigin(chunk), {countPlacement(25), inSquarePlacement(), heightmapPlacement()},
                  [&chunk, &random](const DecorationPos& origin) { placePatchAt(chunk, random, origin); });
    };
    return built;
}

} // namespace basalt::server::world::terrain::decoration
