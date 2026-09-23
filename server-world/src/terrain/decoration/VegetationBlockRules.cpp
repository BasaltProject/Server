#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/VegetationBlockRules.hpp>

#include <cstdint>
#include <optional>
#include <set>
#include <string>

namespace basalt::server::world::terrain::decoration {
namespace {

using PathSet = std::set<std::string, std::less<>>;

/// parity: `BushBlock.mayPlaceOn`, which is the `minecraft:dirt` tag plus farmland.
const PathSet& supportPaths() {
    static const PathSet paths = {"dirt",       "coarse_dirt",     "rooted_dirt", "mud",    "muddy_mangrove_roots",
                                  "moss_block", "pale_moss_block", "grass_block", "podzol", "mycelium",
                                  "farmland"};
    return paths;
}

/// The plants a feature places straight onto the ground, single or the lower half of a double one.
const PathSet& groundPlantPaths() {
    static const PathSet paths = {"short_grass",
                                  "fern",
                                  "bush",
                                  "firefly_bush",
                                  "sweet_berry_bush",
                                  "dandelion",
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
                                  "open_eyeblossom",
                                  "red_shrub",
                                  "tall_grass",
                                  "large_fern",
                                  "sunflower",
                                  "lilac",
                                  "rose_bush",
                                  "peony",
                                  "pitcher_plant"};
    return paths;
}

[[nodiscard]] std::optional<registry::BlockState> stateOf(std::int32_t stateId) {
    return registry::blocks::byStateId(stateId);
}

} // namespace

bool vegetationBlockRules::survivesDeferred(const std::int32_t generatedStateId, const std::int32_t belowStateId) {
    const std::optional<registry::BlockState> generated = stateOf(generatedStateId);
    if (!generated.has_value()) {
        return true;
    }
    const std::string path = generated->block.path();
    if (groundPlantPaths().find(path) == groundPlantPaths().end()) {
        return true;
    }
    const auto half = generated->properties.find("half");
    if (half != generated->properties.end() && half->second == "upper") {
        // The top of a double plant stands on its own bottom, never on the ground.
        const std::optional<registry::BlockState> below = stateOf(belowStateId);
        return below.has_value() && below->block.path() == path;
    }
    const std::optional<registry::BlockState> below = stateOf(belowStateId);
    return below.has_value() && supportPaths().find(below->block.path()) != supportPaths().end();
}

bool vegetationBlockRules::bushMayPlaceOn(const std::int32_t belowStateId) {
    const std::optional<registry::BlockState> below = stateOf(belowStateId);
    return below.has_value() && supportPaths().contains(below->block.path());
}

} // namespace basalt::server::world::terrain::decoration
