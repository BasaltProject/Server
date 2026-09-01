#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>
#include <basalt/server/runtime/world/PlantGrowth.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

/// What each climbing plant leaves behind it. parity: `GrowingPlantHeadBlock.getBodyBlock`.
constexpr std::array<std::pair<std::string_view, std::string_view>, 4> kBodyOf{
    {{"kelp", "kelp_plant"},
     {"cave_vines", "cave_vines_plant"},
     {"twisting_vines", "twisting_vines_plant"},
     {"weeping_vines", "weeping_vines_plant"}}};

} // namespace

void climbingPlantGrowth::growingPlant(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                       std::int32_t stateId, bool upwards, double chance, random::RandomSource& random,
                                       std::vector<BlockChange>& changes) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return;
    }
    // The age cap says what vanilla says; the state table happens to agree, since no climbing
    // plant has a twenty-sixth age to grow into.
    const std::optional<std::int32_t> age = growthSupport::ageOf(stateId);
    if (!age.has_value() || *age >= kMaxAge) {
        return;
    }
    if (random.nextDouble() >= chance) {
        return;
    }
    const std::int32_t nextY = upwards ? y + 1 : y - 1;
    const std::optional<std::int32_t> ahead = world.blockState(x, nextY, z);
    if (!ahead.has_value()) {
        return;
    }
    // parity: `canGrowInto`, which for kelp is water and for the rest is air.
    const bool room = block->path() == "kelp" ? registry::blockSupport::holdsWater(*ahead)
                                              : *ahead == registry::blocks::air().stateId;
    if (!room) {
        return;
    }
    const std::optional<std::int32_t> grown =
        registry::blockStateDefinitions::stateIdOf(*block, {{"age", std::to_string(*age + 1)}}, stateId);
    if (!grown.has_value()) {
        return;
    }
    growthSupport::set(world, x, nextY, z, *grown, changes);

    const auto body = std::ranges::find(kBodyOf, block->path(), &std::pair<std::string_view, std::string_view>::first);
    if (body == kBodyOf.end()) {
        return;
    }
    if (const registry::BlockState* bodyState = registry::blocks::named(body->second); bodyState != nullptr) {
        growthSupport::set(world, x, y, z, bodyState->stateId, changes);
    }
}

void weatheringGrowth::weather(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                               random::RandomSource& random, std::vector<BlockChange>& changes) {
    if (random.nextFloat() >= kDailyChance) {
        return;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return;
    }
    const std::optional<std::int32_t> stage = registry::weathering::stageOf(block->path());
    if (!stage.has_value()) {
        return;
    }

    std::int32_t same = 0;
    std::int32_t older = 0;
    for (std::int32_t dx = -kRange; dx <= kRange; ++dx) {
        for (std::int32_t dy = -kRange; dy <= kRange; ++dy) {
            for (std::int32_t dz = -kRange; dz <= kRange; ++dz) {
                if (dx == 0 && dy == 0 && dz == 0) {
                    continue;
                }
                // parity: a diamond, not a cube: the reach is the sum of the three distances.
                if (std::abs(dx) + std::abs(dy) + std::abs(dz) > kRange) {
                    continue;
                }
                const std::optional<std::int32_t> neighbour = world.blockState(x + dx, y + dy, z + dz);
                if (!neighbour.has_value()) {
                    continue;
                }
                const wire::common::Identifier* theirBlock = registry::blockStateDefinitions::blockOf(*neighbour);
                if (theirBlock == nullptr) {
                    continue;
                }
                const std::optional<std::int32_t> theirs = registry::weathering::stageOf(theirBlock->path());
                if (!theirs.has_value()) {
                    continue;
                }
                // parity: one younger anywhere in reach stops this one weathering at all.
                if (*theirs < *stage) {
                    return;
                }
                if (*theirs > *stage) {
                    ++older;
                } else {
                    ++same;
                }
            }
        }
    }

    const float share = static_cast<float>(older + 1) / static_cast<float>(older + same + 1);
    if (random.nextFloat() >= share * share) {
        return;
    }
    const std::optional<std::string_view> next = registry::weathering::next(block->path());
    if (!next.has_value()) {
        return;
    }
    const wire::common::Identifier nextBlock = wire::common::Identifier::vanilla(std::string{*next});
    std::optional<std::int32_t> weathered = registry::blockStateDefinitions::stateIdOf(
        nextBlock, registry::blockStateDefinitions::propertiesOf(stateId), std::nullopt);
    if (!weathered.has_value()) {
        const registry::BlockState* plain = registry::blocks::named(*next);
        if (plain == nullptr) {
            return;
        }
        weathered = plain->stateId;
    }
    growthSupport::set(world, x, y, z, *weathered, changes);
}

} // namespace basalt::server::runtime::world
