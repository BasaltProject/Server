#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/config/PurpurRules.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/world/Sponges.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

namespace basalt::server::runtime::world::sponges {
namespace {

constexpr std::array<std::array<std::int32_t, 3>, 6> kFaces{{
    {0, -1, 0},
    {0, 1, 0},
    {0, 0, -1},
    {0, 0, 1},
    {-1, 0, 0},
    {1, 0, 0},
}};

/// parity: the plants the search pulls up rather than walking past.
[[nodiscard]] bool isUprooted(std::string_view path) {
    return path == "kelp" || path == "kelp_plant" || path == "seagrass" || path == "tall_seagrass";
}

[[nodiscard]] std::string pathAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const wire::common::Identifier* block =
        registry::blockStateDefinitions::blockOf(world.blockState(x, y, z).value_or(0));
    return block == nullptr ? std::string{} : std::string{block->path()};
}

[[nodiscard]] std::int64_t packed(std::int32_t x, std::int32_t y, std::int32_t z) {
    return ((static_cast<std::int64_t>(x) & 0x3FFFFFF) << 38) | ((static_cast<std::int64_t>(z) & 0x3FFFFFF) << 12) |
           (static_cast<std::int64_t>(y) & 0xFFF);
}

using Drunk = std::array<std::int32_t, 4>;

[[nodiscard]] bool drink(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const OnChanged& onChanged,
                         std::vector<Drunk>& drunk) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return false;
    }
    const std::string_view path = block->path();
    const config::PurpurRules::Blocks& rules = world.gameplay().blocks;
    if (path == "mud" && rules.spongeAbsorbsWaterFromMud) {
        const std::int32_t clay = registry::blocks::named("clay")->stateId;
        if (!world.setBlockState(x, y, z, clay)) {
            return false;
        }
        drunk.push_back({x, y, z, *stateId});
        onChanged(clay, x, y, z);
        return true;
    }
    if (path == "water" || isUprooted(path) || (path == "lava" && rules.spongeAbsorbsLava)) {
        const std::int32_t empty = registry::blocks::air().stateId;
        if (!world.setBlockState(x, y, z, empty)) {
            return false;
        }
        drunk.push_back({x, y, z, *stateId});
        onChanged(empty, x, y, z);
        return true;
    }
    // parity: `BucketPickup.pickupBlock`, which drains a logged block instead of breaking it.
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto waterlogged = properties.find("waterlogged");
    if (waterlogged == properties.end() || waterlogged->second != "true") {
        return false;
    }
    const std::optional<std::int32_t> drained =
        registry::blockStateDefinitions::stateIdOf(*block, {{"waterlogged", "false"}}, *stateId);
    if (!drained.has_value() || !world.setBlockState(x, y, z, *drained)) {
        return false;
    }
    drunk.push_back({x, y, z, *stateId});
    onChanged(*drained, x, y, z);
    return true;
}

} // namespace

void dryIfEvaporating(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const OnChanged& onChanged,
                      bool evaporates) {
    if (!evaporates || pathAt(world, x, y, z) != "wet_sponge") {
        return;
    }
    const std::optional<std::int32_t> dry =
        registry::blockStateDefinitions::stateIdOf(wire::common::Identifier::vanilla("sponge"), {}, std::nullopt);
    if (!dry.has_value() || !world.setBlockState(x, y, z, *dry)) {
        return;
    }
    onChanged(*dry, x, y, z);
}

void absorb(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const OnChanged& onChanged) {
    if (pathAt(world, x, y, z) != "sponge") {
        return;
    }
    std::unordered_set<std::int64_t> visited{packed(x, y, z)};
    std::vector<std::array<std::int32_t, 3>> frontier{{x, y, z}};
    std::int32_t absorbed = 0;
    std::vector<Drunk> drunk;
    const std::int32_t maxAbsorbed = world.gameplay().blocks.spongeAbsorptionArea;
    for (std::int32_t step = 1; step <= world.gameplay().blocks.spongeAbsorptionRadius; ++step) {
        std::vector<std::array<std::int32_t, 3>> next;
        for (const std::array<std::int32_t, 3>& position : frontier) {
            for (const std::array<std::int32_t, 3>& face : kFaces) {
                if (absorbed >= maxAbsorbed) {
                    break;
                }
                const std::int32_t nx = position[0] + face[0];
                const std::int32_t ny = position[1] + face[1];
                const std::int32_t nz = position[2] + face[2];
                if (!visited.insert(packed(nx, ny, nz)).second || !drink(world, nx, ny, nz, onChanged, drunk)) {
                    continue;
                }
                ++absorbed;
                next.push_back({nx, ny, nz});
            }
        }
        if (next.empty() || absorbed >= maxAbsorbed) {
            break;
        }
        frontier = std::move(next);
    }
    if (absorbed == 0) {
        return;
    }
    // A cancelled absorption puts every block back, since the search needs each drained block gone to go on.
    if (!world.events().allowed(&server::plugins::pluginEventTypes::blockSpongeAbsorb,
                                [&](server::plugins::PluginEvent& event) {
                                    std::string lines;
                                    for (const Drunk& at : drunk) {
                                        lines += (lines.empty() ? "" : "\n") + std::to_string(at[0]) + "," +
                                                 std::to_string(at[1]) + "," + std::to_string(at[2]);
                                    }
                                    event.set("world", world.eventWorld());
                                    event.set("x", static_cast<std::int64_t>(x));
                                    event.set("y", static_cast<std::int64_t>(y));
                                    event.set("z", static_cast<std::int64_t>(z));
                                    event.set("blocks", std::move(lines));
                                })) {
        for (const Drunk& at : drunk) {
            if (world.setBlockState(at[0], at[1], at[2], at[3])) {
                onChanged(at[3], at[0], at[1], at[2]);
            }
        }
        return;
    }
    const std::optional<std::int32_t> wet =
        registry::blockStateDefinitions::stateIdOf(wire::common::Identifier::vanilla("wet_sponge"), {}, std::nullopt);
    if (!wet.has_value() || !world.setBlockState(x, y, z, *wet)) {
        return;
    }
    onChanged(*wet, x, y, z);
}

} // namespace basalt::server::runtime::world::sponges
