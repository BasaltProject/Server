#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>
#include <basalt/server/runtime/world/StemGrowth.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

constexpr std::array<std::pair<std::string_view, std::string_view>, 2> kFruitOf{
    {{"pumpkin_stem", "pumpkin"}, {"melon_stem", "melon"}}};

constexpr std::array<std::pair<std::string_view, std::string_view>, 2> kAttachedOf{
    {{"pumpkin_stem", "attached_pumpkin_stem"}, {"melon_stem", "attached_melon_stem"}}};

/// parity: `StemBlock.fruitSupportBlocks`, `#minecraft:dirt` plus farmland.
constexpr std::array<std::string_view, 9> kFruitGround{
    "dirt", "grass_block", "podzol", "coarse_dirt", "mycelium", "rooted_dirt", "moss_block", "mud", "farmland"};

[[nodiscard]] std::string_view lookUp(std::span<const std::pair<std::string_view, std::string_view>> table,
                                      std::string_view key) {
    const auto found = std::ranges::find(table, key, &std::pair<std::string_view, std::string_view>::first);
    return found == table.end() ? std::string_view{} : found->second;
}

} // namespace

void stemGrowth::stem(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                      float speed, random::RandomSource& random, std::vector<BlockChange>& changes, bool hasSkylight) {
    if (worldLight::rawBrightness(world, x, y, z, hasSkylight) < growthSupport::kGrowthLight) {
        return;
    }
    if (random.nextInt(static_cast<std::int32_t>(25.0F / speed) + 1) != 0) {
        return;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return;
    }
    const std::optional<std::int32_t> age = growthSupport::ageOf(stateId);
    if (!age.has_value()) {
        return;
    }
    if (*age < kMaxAge) {
        growthSupport::grow(world, x, y, z, stateId, kMaxAge, changes);
        return;
    }

    const std::string_view facing = growthSupport::kHorizontalNames[static_cast<std::size_t>(
        random.nextInt(static_cast<std::int32_t>(growthSupport::kHorizontalNames.size())))];
    const std::int32_t fruitX = x + growthSupport::offsetX(facing);
    const std::int32_t fruitZ = z + growthSupport::offsetZ(facing);
    if (world.blockState(fruitX, y, fruitZ) != registry::blocks::air().stateId) {
        return;
    }
    const std::optional<std::int32_t> ground = world.blockState(fruitX, y - 1, fruitZ);
    if (!ground.has_value()) {
        return;
    }
    const wire::common::Identifier* groundBlock = registry::blockStateDefinitions::blockOf(*ground);
    if (groundBlock == nullptr || std::ranges::find(kFruitGround, groundBlock->path()) == kFruitGround.end()) {
        return;
    }

    const std::string_view fruit = lookUp(kFruitOf, block->path());
    const std::string_view attached = lookUp(kAttachedOf, block->path());
    if (fruit.empty() || attached.empty()) {
        return;
    }
    const registry::BlockState* fruitState = registry::blocks::named(fruit);
    if (fruitState == nullptr) {
        return;
    }
    const std::optional<std::int32_t> attachedState = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla(std::string{attached}), {{"facing", std::string{facing}}}, std::nullopt);
    if (!attachedState.has_value()) {
        return;
    }
    growthSupport::set(world, fruitX, y, fruitZ, fruitState->stateId, changes);
    growthSupport::set(world, x, y, z, *attachedState, changes);
}

void cocoaGrowth::cocoa(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                        random::RandomSource& random, std::vector<BlockChange>& changes) {
    if (random.nextInt(kOdds) != 0) {
        return;
    }
    growthSupport::grow(world, x, y, z, stateId, kMaxAge, changes);
}

} // namespace basalt::server::runtime::world
