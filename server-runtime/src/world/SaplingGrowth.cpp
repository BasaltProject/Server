#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>
#include <basalt/server/runtime/world/SaplingGrowth.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeConfigs.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeFeature.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::runtime::world::saplingGrowth {
namespace {

namespace trees = server::world::terrain::decoration::tree;

/// Where the scratch chunk puts the sapling, far enough from every edge for any canopy.
constexpr std::int32_t kCentre = 8;

/// How far above the sapling a tree may reach, which bounds what is copied back.
constexpr std::int32_t kHeight = 32;

/// parity: `TreeGrower`, the pairs each sapling picks between and how often it picks the second.
struct Grower {
    const trees::TreeConfiguration* tree = nullptr;
    const trees::TreeConfiguration* secondary = nullptr;
    float secondaryChance = 0.0F;
};

[[nodiscard]] const std::map<std::string, Grower, std::less<>>& growers() {
    static const std::map<std::string, Grower, std::less<>> byPath{
        {"oak_sapling", Grower{&trees::treeConfigs::oak(), &trees::treeConfigs::fancyOak(), 0.1F}},
        {"birch_sapling", Grower{&trees::treeConfigs::birch()}},
        {"spruce_sapling", Grower{&trees::treeConfigs::spruce()}},
        {"jungle_sapling", Grower{&trees::treeConfigs::jungleTree()}},
        {"acacia_sapling", Grower{&trees::treeConfigs::acacia()}},
        {"dark_oak_sapling", Grower{&trees::treeConfigs::darkOak()}},
        {"pale_oak_sapling", Grower{&trees::treeConfigs::paleOak()}},
        {"cherry_sapling", Grower{&trees::treeConfigs::cherry()}},
        {"mangrove_propagule", Grower{&trees::treeConfigs::mangrove()}},
    };
    return byPath;
}

/// Grows `config` on a scratch chunk and copies what it made into the world.
[[nodiscard]] std::vector<BlockChange> plant(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                             const trees::TreeConfiguration& config, random::RandomSource& random) {
    const server::world::Chunk* here = world.chunkIfLoaded(x >> 4, z >> 4);
    if (here == nullptr) {
        return {};
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
    // The sapling itself goes first, as vanilla clears it before placing.
    scratch.setBlockStateId(kCentre, y, kCentre, registry::blocks::air().stateId);

    random::RandomSource worldgen = random::RandomSource::worldgen(random::RandomSource::legacy(random.nextLong()));
    if (!trees::treeFeature::place(scratch, worldgen, kCentre, y, kCentre, config)) {
        return {};
    }

    std::vector<BlockChange> changes;
    std::size_t index = 0;
    for (std::int32_t localX = 0; localX < 16; ++localX) {
        for (std::int32_t localZ = 0; localZ < 16; ++localZ) {
            for (std::int32_t level = lowest; level <= highest; ++level) {
                const std::int32_t after = scratch.blockStateId(localX, level, localZ);
                const std::int32_t was = before[index++];
                if (after == was) {
                    continue;
                }
                growthSupport::set(world, originX + localX, level, originZ + localZ, after, changes);
            }
        }
    }
    return changes;
}

} // namespace

bool isSapling(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && growers().contains(block->path());
}

std::vector<BlockChange> advance(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                 std::int32_t stateId, random::RandomSource& random) {
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

    const trees::TreeConfiguration* config = grower.tree;
    if (grower.secondary != nullptr && random.nextFloat() < grower.secondaryChance) {
        config = grower.secondary;
    }
    return plant(world, x, y, z, *config, random);
}

} // namespace basalt::server::runtime::world::saplingGrowth
