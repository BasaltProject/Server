#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/SimpleBlockFeature.hpp>
#include <basalt/server/world/terrain/decoration/SmallFeatures.hpp>

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kPatchTries = 96;
constexpr float kStrayBlockChance = 0.031F;

using StateProvider = std::function<std::int32_t(random::RandomSource&)>;
using PathSet = std::set<std::string, std::less<>>;

struct Column {
    std::int32_t stateId;
    std::int32_t minHeight;
    std::int32_t maxHeight;
    const PathSet* support;
};

[[nodiscard]] std::int32_t state(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::int32_t stateAt(const Chunk& chunk, const DecorationPos& position) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (position.y < bounds.minY() || position.y > bounds.maxY()) {
        return registry::blocks::air().stateId;
    }
    if ((position.x >> 4) != chunk.chunkX() || (position.z >> 4) != chunk.chunkZ()) {
        return registry::blocks::air().stateId;
    }
    return chunk.blockStateId(position.x & 15, position.y, position.z & 15);
}

[[nodiscard]] bool isAir(const Chunk& chunk, const DecorationPos& position) {
    return registry::blocks::isAir(stateAt(chunk, position));
}

void set(Chunk& chunk, const DecorationPos& position, std::int32_t stateId) {
    if ((position.x >> 4) != chunk.chunkX() || (position.z >> 4) != chunk.chunkZ()) {
        return;
    }
    chunk.setBlockStateId(position.x & 15, position.y, position.z & 15, stateId);
}

[[nodiscard]] StateProvider uniformStates(std::string_view path, std::string_view property,
                                          const std::vector<std::string>& values) {
    std::vector<std::int32_t> states;
    states.reserve(values.size());
    for (const std::string& value : values) {
        const std::optional<std::int32_t> resolved = registry::blocks::stateIdOf(
            wire::common::Identifier::vanilla(std::string{path}), {{std::string{property}, value}});
        states.push_back(resolved.value_or(state(path)));
    }
    return [states = std::move(states)](random::RandomSource& random) {
        return states[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(states.size())))];
    };
}

[[nodiscard]] StateProvider flowerBedStates(std::string_view path, std::string_view amountProperty,
                                            std::int32_t maxAmount) {
    static constexpr std::array<std::string_view, 4> kFacings{"north", "east", "south", "west"};
    std::vector<std::int32_t> states;
    for (std::int32_t amount = 1; amount <= maxAmount; ++amount) {
        for (const std::string_view facing : kFacings) {
            const std::optional<std::int32_t> resolved = registry::blocks::stateIdOf(
                wire::common::Identifier::vanilla(std::string{path}),
                {{"facing", std::string{facing}}, {std::string{amountProperty}, std::to_string(amount)}});
            if (!resolved.has_value()) {
                throw std::runtime_error{"unknown " + std::string{path} + " state"};
            }
            states.push_back(*resolved);
        }
    }
    return [states = std::move(states)](random::RandomSource& random) {
        return states[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(states.size())))];
    };
}

const std::map<std::string, StateProvider, std::less<>>& singleBlocks() {
    static const std::map<std::string, StateProvider, std::less<>> built = [] {
        std::map<std::string, StateProvider, std::less<>> map;
        map.emplace("patch_leaf_litter", flowerBedStates("leaf_litter", "segment_amount", 3));
        map.emplace("wildflowers_birch_forest", flowerBedStates("wildflowers", "flower_amount", 4));
        map.emplace("wildflowers_meadow", flowerBedStates("wildflowers", "flower_amount", 4));
        for (const std::string_view name :
             {"patch_firefly_bush_near_water", "patch_firefly_bush_near_water_swamp", "patch_firefly_bush_swamp"}) {
            map.emplace(name, [](random::RandomSource&) { return state("firefly_bush"); });
        }
        return map;
    }();
    return built;
}

const std::map<std::string, StateProvider, std::less<>>& ceilingBlocks() {
    static const std::map<std::string, StateProvider, std::less<>> built = [] {
        std::map<std::string, StateProvider, std::less<>> map;
        map.emplace("spore_blossom", [](random::RandomSource&) { return state("spore_blossom"); });
        return map;
    }();
    return built;
}

const std::map<std::string, Column, std::less<>>& columns() {
    static const PathSet sugarCaneSupport = {"grass_block", "dirt",     "coarse_dirt", "podzol",
                                             "sand",        "red_sand", "moss_block"};
    static const std::map<std::string, Column, std::less<>> built = [] {
        // parity: `patch_sugar_cane`, two to four tall biased to the bottom, on sand, dirt or moss.
        const Column sugarCane{
            .stateId = state("sugar_cane"), .minHeight = 2, .maxHeight = 4, .support = &sugarCaneSupport};
        std::map<std::string, Column, std::less<>> map;
        for (const std::string_view name :
             {"patch_sugar_cane", "patch_sugar_cane_desert", "patch_sugar_cane_badlands", "patch_sugar_cane_swamp"}) {
            map.emplace(name, sugarCane);
        }
        return map;
    }();
    return built;
}

const std::map<std::string, StateProvider, std::less<>>& piles() {
    static const std::map<std::string, StateProvider, std::less<>> built = [] {
        std::map<std::string, StateProvider, std::less<>> map;
        map.emplace("pile_hay", uniformStates("hay_block", "axis", {"x", "y", "z"}));
        map.emplace("pile_snow", [](random::RandomSource&) { return state("snow"); });
        map.emplace("pile_melon", [](random::RandomSource&) { return state("melon"); });
        map.emplace("pile_ice", [](random::RandomSource& random) {
            return random.nextInt(6) == 0 ? state("blue_ice") : state("packed_ice");
        });
        map.emplace("pile_pumpkin", [](random::RandomSource& random) {
            return random.nextInt(20) == 19 ? state("jack_o_lantern") : state("pumpkin");
        });
        return map;
    }();
    return built;
}

/// parity: `random_patch` around the origin, 96 tries with the usual 7/3 spread.
bool placePatch(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                const StateProvider& stateProvider) {
    const std::int32_t minY = chunk.verticalBounds().minY();
    bool placed = false;
    for (std::int32_t attempt = 0; attempt < kPatchTries; ++attempt) {
        const DecorationPos target{origin.x + random.nextInt(8) - random.nextInt(8),
                                   origin.y + random.nextInt(4) - random.nextInt(4),
                                   origin.z + random.nextInt(8) - random.nextInt(8)};
        const std::int32_t stateId = stateProvider(random);
        if (!isAir(chunk, target) || target.y <= minY) {
            continue;
        }
        const SimpleBlockFeature feature{[stateId](random::RandomSource&, const DecorationPos&) { return stateId; }};
        if (feature.place(chunk, random, target)) {
            placed = true;
        }
    }
    return placed;
}

/// parity: `spore_blossom`, which hangs from the block above rather than standing on one.
bool placeUnderCeiling(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                       const StateProvider& stateProvider) {
    if (!isAir(chunk, origin) || origin.y + 1 > chunk.verticalBounds().maxY()) {
        return false;
    }
    if (!registry::blockShapes::supportsCentre(stateAt(chunk, DecorationPos{origin.x, origin.y + 1, origin.z}),
                                               registry::BlockFace::Down)) {
        return false;
    }
    set(chunk, origin, stateProvider(random));
    return true;
}

/// parity: `BlockColumnFeature`, a stack of one block grown upward while there is room.
bool placeColumn(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, const Column& column) {
    if (!isAir(chunk, origin)) {
        return false;
    }
    const std::int32_t below = stateAt(chunk, DecorationPos{origin.x, origin.y - 1, origin.z});
    if (!column.support->contains(pathOf(below))) {
        return false;
    }
    // parity: `UniformInt.biasedToBottom`, which is what sugar cane's own height provider is.
    const std::int32_t height =
        column.minHeight + random.nextInt(random.nextInt(column.maxHeight - column.minHeight + 1) + 1);
    const std::int32_t maxY = chunk.verticalBounds().maxY();
    std::int32_t placed = 0;
    while (placed < height) {
        const DecorationPos target{origin.x, origin.y + placed, origin.z};
        if (target.y > maxY || !isAir(chunk, target)) {
            break;
        }
        set(chunk, target, column.stateId);
        ++placed;
    }
    return placed > 0;
}

void tryPlacePile(Chunk& chunk, random::RandomSource& random, const DecorationPos& target,
                  const StateProvider& stateProvider) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (target.y < bounds.minY() || target.y > bounds.maxY() || !isAir(chunk, target)) {
        return;
    }
    const std::int32_t below = stateAt(chunk, DecorationPos{target.x, target.y - 1, target.z});
    // parity: a dirt path holds a pile only half the time; anything else needs a solid top face.
    const bool holds = pathOf(below) == "dirt_path"
                           ? random.nextBoolean()
                           : registry::blockShapes::isFaceSturdy(below, registry::BlockFace::Up);
    if (!holds) {
        return;
    }
    set(chunk, target, stateProvider(random));
}

/// parity: `BlockPileFeature`, a squashed blob whose radius is drawn per column.
bool placePile(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
               const StateProvider& stateProvider) {
    if (origin.y < chunk.verticalBounds().minY() + 5) {
        return false;
    }
    const std::int32_t radiusX = 2 + random.nextInt(2);
    const std::int32_t radiusZ = 2 + random.nextInt(2);
    for (std::int32_t z = origin.z - radiusZ; z <= origin.z + radiusZ; ++z) {
        for (std::int32_t y = origin.y; y <= origin.y + 1; ++y) {
            for (std::int32_t x = origin.x - radiusX; x <= origin.x + radiusX; ++x) {
                const std::int32_t dx = origin.x - x;
                const std::int32_t dz = origin.z - z;
                const bool inside =
                    static_cast<float>(dx * dx + dz * dz) <= random.nextFloat() * 10.0F - random.nextFloat() * 6.0F;
                if (inside || random.nextFloat() < kStrayBlockChance) {
                    tryPlacePile(chunk, random, DecorationPos{x, y, z}, stateProvider);
                }
            }
        }
    }
    return true;
}

} // namespace

const std::set<std::string, std::less<>>& smallFeatures::placedIds() {
    static const std::set<std::string, std::less<>> ids = [] {
        std::set<std::string, std::less<>> built;
        for (const auto& [name, provider] : singleBlocks()) {
            built.insert(name);
        }
        for (const auto& [name, column] : columns()) {
            built.insert(name);
        }
        for (const auto& [name, provider] : piles()) {
            built.insert(name);
        }
        for (const auto& [name, provider] : ceilingBlocks()) {
            built.insert(name);
        }
        return built;
    }();
    return ids;
}

bool smallFeatures::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                            const std::string_view placedFeature) {
    if (const auto found = singleBlocks().find(placedFeature); found != singleBlocks().end()) {
        return placePatch(chunk, random, origin, found->second);
    }
    if (const auto found = ceilingBlocks().find(placedFeature); found != ceilingBlocks().end()) {
        return placeUnderCeiling(chunk, random, origin, found->second);
    }
    if (const auto found = columns().find(placedFeature); found != columns().end()) {
        return placeColumn(chunk, random, origin, found->second);
    }
    if (const auto found = piles().find(placedFeature); found != piles().end()) {
        return placePile(chunk, random, origin, found->second);
    }
    return false;
}

} // namespace basalt::server::world::terrain::decoration
