#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/world/ClimbingPlantGrowth.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::runtime::world {
namespace {

[[nodiscard]] std::optional<std::int32_t> ageOf(std::int32_t stateId) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find("age");
    if (found == properties.end()) {
        return std::nullopt;
    }
    std::int32_t value = 0;
    const std::from_chars_result result =
        std::from_chars(found->second.data(), found->second.data() + found->second.size(), value);
    if (result.ec != std::errc{} || result.ptr != found->second.data() + found->second.size()) {
        return std::nullopt;
    }
    return value;
}

/// What each climbing plant leaves behind it. parity: `GrowingPlantHeadBlock.getBodyBlock`.
[[nodiscard]] std::string_view bodyOf(std::string_view head) {
    if (head == "kelp") {
        return "kelp_plant";
    }
    if (head == "cave_vines") {
        return "cave_vines_plant";
    }
    if (head == "twisting_vines") {
        return "twisting_vines_plant";
    }
    if (head == "weeping_vines") {
        return "weeping_vines_plant";
    }
    return {};
}

} // namespace

namespace climbingPlantGrowth {

void growingPlant(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, bool upwards,
                  double chance, random::RandomSource& random, std::vector<BlockChange>& changes) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return;
    }
    const std::optional<std::int32_t> age = ageOf(stateId);
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
    const std::string_view body = bodyOf(block->path());
    if (body.empty()) {
        return;
    }
    const registry::BlockState* left = registry::blocks::named(body);
    if (left != nullptr) {
        growthSupport::set(world, x, y, z, left->stateId, changes);
    }
}

} // namespace climbingPlantGrowth

namespace weatheringGrowth {

void weather(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
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
                if (std::abs(dx) + std::abs(dy) + std::abs(dz) > kRange) {
                    continue;
                }
                const std::optional<std::int32_t> neighbour = world.blockState(x + dx, y + dy, z + dz);
                if (!neighbour.has_value()) {
                    continue;
                }
                const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(*neighbour);
                if (id == nullptr) {
                    continue;
                }
                const std::optional<std::int32_t> theirs = registry::weathering::stageOf(id->path());
                if (!theirs.has_value()) {
                    continue;
                }
                // parity: one neighbour younger than this block stops it weathering at all.
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
    // parity: `ChangeOverTimeBlock.getNextState` is `chance * chance * getChanceModifier()`, and
    // `WeatheringCopper.getChanceModifier` is **three quarters while the block is unaffected** and one
    // afterwards. Without it a fresh block of copper weathered a third too fast, which is what a
    // lightning rod turning `exposed` inside twenty ticks on the bench said.
    const float modifier = *stage == 0 ? kUnaffectedChanceModifier : 1.0F;
    if (random.nextFloat() >= share * share * modifier) {
        return;
    }
    const std::optional<std::string_view> next = registry::weathering::next(block->path());
    if (!next.has_value()) {
        return;
    }
    const wire::common::Identifier id = wire::common::Identifier::vanilla(std::string{*next});
    std::optional<std::int32_t> weathered = registry::blockStateDefinitions::stateIdOf(
        id, registry::blockStateDefinitions::propertiesOf(stateId), std::nullopt);
    if (!weathered.has_value()) {
        const registry::BlockState* plain = registry::blocks::named(*next);
        if (plain == nullptr) {
            return;
        }
        weathered = plain->stateId;
    }
    growthSupport::set(world, x, y, z, *weathered, changes);
}

} // namespace weatheringGrowth

} // namespace basalt::server::runtime::world
