#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>
#include <basalt/server/runtime/world/SaplingGrowth.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeConfigs.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeFeature.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::runtime::world::saplingGrowth {
namespace {

namespace trees = server::world::terrain::decoration::tree;

constexpr std::int32_t kCentre = 8;

constexpr std::int32_t kHeight = 32;

struct Weighted {
    const trees::TreeConfiguration* tree = nullptr;
    std::int32_t weight = 1;
};

using WeightedTrees = std::vector<Weighted>;

/// parity: 26.3 `TreeGrower`.
struct Grower {
    WeightedTrees trees;
    WeightedTrees megaTrees;
    WeightedTrees flowerTrees;
};

/// parity: `TreeFeatures.BIRCH_BEES_005`, the birch a sapling grows beside flowers.
[[nodiscard]] const trees::TreeConfiguration& birchBees005() {
    static const trees::TreeConfiguration config = [] {
        trees::TreeConfiguration birch = trees::treeConfigs::birch();
        birch.decorators = {trees::decorator::Beehive{.probability = 0.05F}};
        return birch;
    }();
    return config;
}

[[nodiscard]] const std::map<std::string, Grower, std::less<>>& growers() {
    namespace configs = trees::treeConfigs;
    static const std::map<std::string, Grower, std::less<>> byPath{
        {"oak_sapling", Grower{.trees = {{&configs::oak(), 9}, {&configs::fancyOak(), 1}},
                               .flowerTrees = {{&configs::oakBees005(), 9}, {&configs::fancyOakBees005(), 1}}}},
        {"spruce_sapling",
         Grower{.trees = {{&configs::spruce()}}, .megaTrees = {{&configs::megaSpruce()}, {&configs::megaPine()}}}},
        {"mangrove_propagule", Grower{.trees = {{&configs::mangrove(), 15}, {&configs::tallMangrove(), 85}}}},
        {"birch_sapling", Grower{.trees = {{&configs::birch()}}, .flowerTrees = {{&birchBees005()}}}},
        {"jungle_sapling", Grower{.trees = {{&configs::jungleTree()}}, .megaTrees = {{&configs::megaJungle()}}}},
        {"acacia_sapling", Grower{.trees = {{&configs::acacia()}}}},
        {"cherry_sapling", Grower{.trees = {{&configs::cherry()}}, .flowerTrees = {{&configs::cherryBees005()}}}},
        {"dark_oak_sapling", Grower{.megaTrees = {{&configs::darkOak()}}}},
        {"pale_oak_sapling", Grower{.megaTrees = {{&configs::paleOak()}}}},
        {"poplar_sapling",
         Grower{.trees = {{&configs::redPoplar()}, {&configs::orangePoplar()}, {&configs::yellowPoplar()}}}},
    };
    return byPath;
}

/// parity: `WeightedList.getRandom`.
[[nodiscard]] std::optional<const trees::TreeConfiguration*> pick(const WeightedTrees& list,
                                                                  random::RandomSource& random) {
    std::int32_t total = 0;
    for (const Weighted& entry : list) {
        total += entry.weight;
    }
    if (total <= 0) {
        return std::nullopt;
    }
    std::int32_t selection = random.nextInt(total);
    for (const Weighted& entry : list) {
        if (selection < entry.weight) {
            return entry.tree;
        }
        selection -= entry.weight;
    }
    return std::nullopt;
}

/// parity: `TreeGrower.hasFlowers`, the box two out and one up or down.
[[nodiscard]] bool hasFlowers(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                              const Flowering& flowering) {
    if (!flowering) {
        return false;
    }
    for (std::int32_t dx = -2; dx <= 2; ++dx) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dz = -2; dz <= 2; ++dz) {
                const std::optional<std::int32_t> state = world.blockState(x + dx, y + dy, z + dz);
                const wire::common::Identifier* block =
                    state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
                if (block != nullptr && flowering(*block)) {
                    return true;
                }
            }
        }
    }
    return false;
}

/// parity: `TreeGrower.findTwoByTwoSaplingPos`.
[[nodiscard]] std::optional<std::array<std::int32_t, 2>> twoByTwo(const WorldState& world, std::int32_t x,
                                                                  std::int32_t y, std::int32_t z,
                                                                  const wire::common::Identifier& block) {
    const auto same = [&](std::int32_t atX, std::int32_t atZ) {
        const std::optional<std::int32_t> state = world.blockState(atX, y, atZ);
        const wire::common::Identifier* here =
            state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
        return here != nullptr && *here == block;
    };
    for (std::int32_t dx = 0; dx >= -1; --dx) {
        for (std::int32_t dz = 0; dz >= -1; --dz) {
            if (same(x + dx, z + dz) && same(x + dx + 1, z + dz) && same(x + dx, z + dz + 1) &&
                same(x + dx + 1, z + dz + 1)) {
                return std::array<std::int32_t, 2>{dx, dz};
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::vector<PlannedBlock>> planConfig(WorldState& world, std::int32_t x, std::int32_t y,
                                                                  std::int32_t z,
                                                                  const trees::TreeConfiguration& config,
                                                                  random::RandomSource& random, std::int32_t clear) {
    const server::world::Chunk* here = world.chunkIfLoaded(x >> 4, z >> 4);
    if (here == nullptr) {
        return std::nullopt;
    }
    const server::world::ChunkHeight bounds = here->verticalBounds();
    const std::int32_t originX = x - kCentre;
    const std::int32_t originZ = z - kCentre;
    const std::int32_t lowest = std::max(bounds.minY(), y - 1);
    const std::int32_t highest = std::min(bounds.maxY(), y + kHeight);

    server::world::Chunk scratch{0, 0, bounds};
    std::vector<std::int32_t> before;
    for (std::int32_t localX = 0; localX < 16; ++localX) {
        for (std::int32_t localZ = 0; localZ < 16; ++localZ) {
            for (std::int32_t level = lowest; level <= highest; ++level) {
                const std::int32_t stateId = world.blockState(originX + localX, level, originZ + localZ).value_or(0);
                scratch.setBlockStateId(localX, level, localZ, stateId);
                before.push_back(stateId);
            }
        }
    }
    // The sapling itself goes first, as vanilla clears it before placing; a 2x2 clears all four.
    if (clear >= 1) {
        scratch.setBlockStateId(kCentre, y, kCentre, registry::blocks::air().stateId);
    }
    if (clear >= 2) {
        scratch.setBlockStateId(kCentre + 1, y, kCentre, registry::blocks::air().stateId);
        scratch.setBlockStateId(kCentre, y, kCentre + 1, registry::blocks::air().stateId);
        scratch.setBlockStateId(kCentre + 1, y, kCentre + 1, registry::blocks::air().stateId);
    }

    random::RandomSource worldgen = random::RandomSource::worldgen(random::RandomSource::legacy(random.nextLong()));
    if (!trees::treeFeature::place(scratch, worldgen, kCentre, y, kCentre, config)) {
        return std::nullopt;
    }

    std::vector<PlannedBlock> planned;
    std::size_t index = 0;
    for (std::int32_t localX = 0; localX < 16; ++localX) {
        for (std::int32_t localZ = 0; localZ < 16; ++localZ) {
            for (std::int32_t level = lowest; level <= highest; ++level) {
                const std::int32_t after = scratch.blockStateId(localX, level, localZ);
                const std::int32_t was = before[index++];
                if (after != was) {
                    planned.push_back(PlannedBlock{
                        .x = originX + localX, .y = level, .z = originZ + localZ, .before = was, .after = after});
                }
            }
        }
    }
    return planned;
}

[[nodiscard]] std::vector<BlockChange> plant(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                             const trees::TreeConfiguration& config, random::RandomSource& random,
                                             bool square) {
    const std::optional<std::vector<PlannedBlock>> planned = planConfig(world, x, y, z, config, random, square ? 2 : 1);
    std::vector<BlockChange> changes;
    for (const PlannedBlock& block : planned.value_or(std::vector<PlannedBlock>{})) {
        growthSupport::set(world, block.x, block.y, block.z, block.after, changes);
    }
    return changes;
}

} // namespace

bool isSapling(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && growers().contains(block->path());
}

std::vector<BlockChange> advance(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                 std::int32_t stateId, random::RandomSource& random, const Flowering& flowering,
                                 const MayGrow& mayGrow) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return {};
    }
    const auto found = growers().find(block->path());
    if (found == growers().end()) {
        return {};
    }
    const Grower& grower = found->second;

    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto stage = properties.find("stage");
    if (stage != properties.end() && stage->second == "0") {
        const std::optional<std::int32_t> grown =
            registry::blockStateDefinitions::stateIdOf(*block, {{"stage", "1"}}, stateId);
        if (!grown.has_value()) {
            return {};
        }
        std::vector<BlockChange> changes;
        growthSupport::set(world, x, y, z, *grown, changes);
        return changes;
    }

    if (mayGrow && !mayGrow()) {
        return {};
    }
    // parity: `TreeGrower.growTree`.
    if (const std::optional<const trees::TreeConfiguration*> mega = pick(grower.megaTrees, random);
        mega.has_value() && *mega != nullptr) {
        if (const std::optional<std::array<std::int32_t, 2>> corner = twoByTwo(world, x, y, z, *block);
            corner.has_value()) {
            return plant(world, x + (*corner)[0], y, z + (*corner)[1], **mega, random, true);
        }
    }
    const bool flowers = !grower.flowerTrees.empty() && hasFlowers(world, x, y, z, flowering);
    const std::optional<const trees::TreeConfiguration*> config =
        pick(flowers ? grower.flowerTrees : grower.trees, random);
    if (!config.has_value() || *config == nullptr) {
        return {};
    }
    return plant(world, x, y, z, **config, random, false);
}

std::optional<std::vector<PlannedBlock>> planTree(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::string_view tree, random::RandomSource& random) {
    namespace configs = trees::treeConfigs;
    static const std::map<std::string, const trees::TreeConfiguration*, std::less<>> byName{
        {"oak", &configs::oak()},
        {"fancy_oak", &configs::fancyOak()},
        {"swamp_oak", &configs::swampOak()},
        {"azalea", &configs::azalea()},
        {"birch", &configs::birch()},
        {"super_birch", &configs::superBirch()},
        {"jungle_tree", &configs::jungleTree()},
        {"jungle_bush", &configs::jungleBush()},
        {"mega_jungle", &configs::megaJungle()},
        {"spruce", &configs::spruce()},
        {"pine", &configs::pine()},
        {"acacia", &configs::acacia()},
        {"dark_oak", &configs::darkOak()},
        {"pale_oak", &configs::paleOak()},
        {"pale_oak_creaking", &configs::paleOakCreaking()},
        {"mega_spruce", &configs::megaSpruce()},
        {"mega_pine", &configs::megaPine()},
        {"mangrove", &configs::mangrove()},
        {"tall_mangrove", &configs::tallMangrove()},
        {"cherry", &configs::cherry()},
    };
    const auto found = byName.find(tree);
    if (found == byName.end()) {
        return std::nullopt;
    }
    return planConfig(world, x, y, z, *found->second, random, 0);
}

} // namespace basalt::server::runtime::world::saplingGrowth
