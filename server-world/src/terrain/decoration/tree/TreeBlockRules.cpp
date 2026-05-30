#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeBlockRules.hpp>

#include <cstdint>
#include <map>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::decoration::tree {
namespace {

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

/// parity: `minecraft:replaceable_by_trees`, flattened from its leaf and flower tags.
const PathSet& replaceableByTreesPaths() {
    static const PathSet paths = {"jungle_leaves",
                                  "oak_leaves",
                                  "spruce_leaves",
                                  "pale_oak_leaves",
                                  "dark_oak_leaves",
                                  "acacia_leaves",
                                  "birch_leaves",
                                  "azalea_leaves",
                                  "flowering_azalea_leaves",
                                  "mangrove_leaves",
                                  "cherry_leaves",
                                  "dandelion",
                                  "open_eyeblossom",
                                  "poppy",
                                  "blue_orchid",
                                  "allium",
                                  "azure_bluet",
                                  "red_tulip",
                                  "orange_tulip",
                                  "white_tulip",
                                  "pink_tulip",
                                  "oxeye_daisy",
                                  "cornflower",
                                  "lily_of_the_valley",
                                  "wither_rose",
                                  "torchflower",
                                  "closed_eyeblossom",
                                  "golden_dandelion",
                                  "pale_moss_carpet",
                                  "short_grass",
                                  "fern",
                                  "dead_bush",
                                  "vine",
                                  "glow_lichen",
                                  "sunflower",
                                  "lilac",
                                  "rose_bush",
                                  "peony",
                                  "tall_grass",
                                  "large_fern",
                                  "hanging_roots",
                                  "pitcher_plant",
                                  "water",
                                  "seagrass",
                                  "tall_seagrass",
                                  "bush",
                                  "firefly_bush",
                                  "warped_roots",
                                  "nether_sprouts",
                                  "crimson_roots",
                                  "leaf_litter",
                                  "short_dry_grass",
                                  "tall_dry_grass",
                                  "orange_poplar_leaves",
                                  "red_poplar_leaves",
                                  "yellow_poplar_leaves",
                                  "shelf_mushroom"};
    return paths;
}

/// parity: the `MangroveRootPlacement` tags `MangroveRootPlacer` uses.
const PathSet& mangroveRootGrowThroughPaths() {
    static const PathSet paths = {
        "mud", "muddy_mangrove_roots", "mangrove_roots", "moss_carpet", "vine", "mangrove_propagule", "snow"};
    return paths;
}

const PathSet& muddyRootsInPaths() {
    static const PathSet paths = {"mud", "muddy_mangrove_roots"};
    return paths;
}

/// Source water only: a flowing level would carry the wrong fluid into the leaf state.
[[nodiscard]] bool hasSourceWaterFluid(std::int32_t stateId) {
    const std::int32_t water = registry::blocks::water().stateId;
    return registry::blockFluids::hasWaterFluid(stateId) && !(stateId > water && stateId <= water + 15);
}

} // namespace

const PathSet& treeBlockRules::logBlockPaths() {
    static const PathSet paths = [] {
        PathSet built;
        for (const std::string_view wood :
             {"dark_oak", "pale_oak", "oak", "acacia", "birch", "jungle", "spruce", "mangrove", "cherry", "poplar"}) {
            built.emplace(std::string{wood} + "_log");
            built.emplace(std::string{wood} + "_wood");
            built.emplace("stripped_" + std::string{wood} + "_log");
            built.emplace("stripped_" + std::string{wood} + "_wood");
        }
        for (const std::string_view extra :
             {"crimson_stem", "stripped_crimson_stem", "crimson_hyphae", "stripped_crimson_hyphae", "warped_stem",
              "stripped_warped_stem", "warped_hyphae", "stripped_warped_hyphae"}) {
            built.emplace(extra);
        }
        return built;
    }();
    return paths;
}

namespace {

template <typename Answer>
[[nodiscard]] bool tabled(std::int32_t stateId, const std::vector<bool>& table, Answer&& answer) {
    return stateId >= 0 && static_cast<std::size_t>(stateId) < table.size() ? table[static_cast<std::size_t>(stateId)]
                                                                            : answer(stateId);
}

} // namespace

bool treeBlockRules::isLogState(std::int32_t stateId) {
    static const std::vector<bool> table = [] {
        std::vector<bool> built(static_cast<std::size_t>(registry::blocks::stateCount()));
        for (std::size_t id = 0; id < built.size(); ++id) {
            built[id] = logBlockPaths().find(pathOf(static_cast<std::int32_t>(id))) != logBlockPaths().end();
        }
        return built;
    }();
    return tabled(stateId, table,
                  [](std::int32_t id) { return logBlockPaths().find(pathOf(id)) != logBlockPaths().end(); });
}

bool treeBlockRules::isValidTreeState(std::int32_t stateId) {
    static const std::vector<bool> table = [] {
        std::vector<bool> built(static_cast<std::size_t>(registry::blocks::stateCount()));
        for (std::size_t id = 0; id < built.size(); ++id) {
            const auto state = static_cast<std::int32_t>(id);
            built[id] = registry::blocks::isAir(state) ||
                        replaceableByTreesPaths().find(pathOf(state)) != replaceableByTreesPaths().end();
        }
        return built;
    }();
    return tabled(stateId, table, [](std::int32_t id) {
        return registry::blocks::isAir(id) ||
               replaceableByTreesPaths().find(pathOf(id)) != replaceableByTreesPaths().end();
    });
}

bool treeBlockRules::canReplaceDeferred(std::int32_t actualStateId, std::int32_t generatedStateId) {
    const std::string actualPath = pathOf(actualStateId);
    if (isLogState(generatedStateId)) {
        return isValidTreeState(actualStateId);
    }
    if (registry::blockStateFamilies::isLeavesState(generatedStateId)) {
        // A persistent leaf is player-placed and is never overwritten by generation.
        const std::map<std::string, std::string> properties =
            registry::blockStateDefinitions::propertiesOf(actualStateId);
        const auto persistent = properties.find("persistent");
        const bool isPersistent = persistent != properties.end() && persistent->second == "true";
        return !isPersistent && isValidTreeState(actualStateId);
    }
    const std::string generatedPath = pathOf(generatedStateId);
    if (generatedPath == "mangrove_roots") {
        return isValidTreeState(actualStateId) ||
               mangroveRootGrowThroughPaths().find(actualPath) != mangroveRootGrowThroughPaths().end();
    }
    if (generatedPath == "muddy_mangrove_roots") {
        return muddyRootsInPaths().find(actualPath) != muddyRootsInPaths().end();
    }
    return false;
}

std::int32_t treeBlockRules::resolveDeferredState(std::int32_t actualStateId, std::int32_t generatedStateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(generatedStateId);
    if (block == nullptr) {
        return generatedStateId;
    }
    std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(generatedStateId);
    if (!properties.contains("waterlogged")) {
        return generatedStateId;
    }
    // parity: leaves take source water only, everything else takes any water level.
    const bool waterlogged = registry::blockStateFamilies::isLeavesState(generatedStateId)
                                 ? hasSourceWaterFluid(actualStateId)
                                 : registry::blockFluids::hasWaterFluid(actualStateId);
    properties["waterlogged"] = waterlogged ? "true" : "false";
    const std::optional<std::int32_t> resolved = registry::blocks::stateIdOf(*block, properties);
    return resolved.value_or(generatedStateId);
}

} // namespace basalt::server::world::terrain::decoration::tree
