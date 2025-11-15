#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Sponges.hpp>

#include <array>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

namespace basalt::server::runtime::world::sponges {
namespace {

/// parity: `SpongeBlock`'s own two limits.
constexpr std::int32_t kReach = 6;
constexpr std::int32_t kMaxAbsorbed = 65;

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

/// Takes the water out of one position, answering whether there was any to take.
[[nodiscard]] bool drink(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                         const OnChanged& onChanged) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return false;
    }
    const std::string_view path = block->path();
    if (path == "water" || isUprooted(path)) {
        const std::int32_t empty = registry::blocks::air().stateId;
        if (!world.setBlockState(x, y, z, empty)) {
            return false;
        }
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
    for (std::int32_t step = 1; step <= kReach; ++step) {
        std::vector<std::array<std::int32_t, 3>> next;
        for (const std::array<std::int32_t, 3>& position : frontier) {
            for (const std::array<std::int32_t, 3>& face : kFaces) {
                if (absorbed >= kMaxAbsorbed) {
                    break;
                }
                const std::int32_t nx = position[0] + face[0];
                const std::int32_t ny = position[1] + face[1];
                const std::int32_t nz = position[2] + face[2];
                if (!visited.insert(packed(nx, ny, nz)).second || !drink(world, nx, ny, nz, onChanged)) {
                    continue;
                }
                ++absorbed;
                next.push_back({nx, ny, nz});
            }
        }
        if (next.empty() || absorbed >= kMaxAbsorbed) {
            break;
        }
        frontier = std::move(next);
    }
    if (absorbed == 0) {
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
