#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/config/WorldRules.hpp>
#include <basalt/server/runtime/world/BambooGrowth.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>

#include <cstdint>
#include <string>

namespace basalt::server::runtime::world::bambooGrowth {
namespace {

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& key) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] bool isBamboo(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    return block != nullptr && block->path() == "bamboo";
}

void shrink(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
            const std::string& leaves, std::vector<BlockChange>& changes) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return;
    }
    const std::optional<std::int32_t> smaller =
        registry::blockStateDefinitions::stateIdOf(*block, {{"leaves", leaves}}, stateId);
    if (!smaller.has_value()) {
        return;
    }
    growthSupport::set(world, x, y, z, *smaller, changes);
}

} // namespace

void bamboo(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
            random::RandomSource& random, std::vector<BlockChange>& changes, bool hasSkylight) {
    // parity: a stalk at stage one has finished and never grows again.
    if (propertyOf(stateId, "stage") == "1") {
        return;
    }
    if (random.nextInt(kOdds) != 0) {
        return;
    }
    if (world.blockState(x, y + 1, z) != registry::blocks::air().stateId) {
        return;
    }
    if (worldLight::rawBrightness(world, x, y + 1, z, hasSkylight) < growthSupport::kGrowthLight) {
        return;
    }

    std::int32_t height = 1;
    while (isBamboo(world, x, y - height, z)) {
        ++height;
    }
    if (height >= world.rules().tuning.bambooMaxHeight) {
        return;
    }

    const std::optional<std::int32_t> below = world.blockState(x, y - 1, z);
    const std::optional<std::int32_t> twoBelow = world.blockState(x, y - 2, z);
    if (!below.has_value() || !twoBelow.has_value()) {
        return;
    }

    // parity: the leaves walk up with the tip, so the two below it lose theirs as it passes.
    std::string leaves = "none";
    if (!isBamboo(world, x, y - 1, z) || propertyOf(*below, "leaves") == "none") {
        leaves = "small";
    } else {
        leaves = "large";
        if (isBamboo(world, x, y - 2, z)) {
            shrink(world, x, y - 1, z, *below, "small", changes);
            shrink(world, x, y - 2, z, *twoBelow, "none", changes);
        }
    }

    const std::string age = propertyOf(stateId, "age") != "1" && !isBamboo(world, x, y - 2, z) ? "0" : "1";
    const bool keepsGrowing = (height < kTall || random.nextFloat() >= kStageChance) && height != 15;
    const std::optional<std::int32_t> grown = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla("bamboo"),
        {{"age", age}, {"leaves", leaves}, {"stage", keepsGrowing ? "0" : "1"}}, std::nullopt);
    if (!grown.has_value()) {
        return;
    }
    growthSupport::set(world, x, y + 1, z, *grown, changes);
}

void bambooSapling(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                   std::vector<BlockChange>& changes, bool hasSkylight) {
    if (random.nextInt(kOdds) != 0) {
        return;
    }
    if (world.blockState(x, y + 1, z) != registry::blocks::air().stateId) {
        return;
    }
    if (worldLight::rawBrightness(world, x, y + 1, z, hasSkylight) < growthSupport::kGrowthLight) {
        return;
    }
    const registry::BlockState* stalk = registry::blocks::named("bamboo");
    if (stalk == nullptr) {
        return;
    }
    growthSupport::set(world, x, y + 1, z, stalk->stateId, changes);
}

} // namespace basalt::server::runtime::world::bambooGrowth
