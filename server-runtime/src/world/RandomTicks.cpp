#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/world/BambooGrowth.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/ChorusFlowerGrowth.hpp>
#include <basalt/server/runtime/world/ClimbingPlantGrowth.hpp>
#include <basalt/server/runtime/world/DripstoneGrowth.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>
#include <basalt/server/runtime/world/LavaGrowth.hpp>
#include <basalt/server/runtime/world/MiscGrowth.hpp>
#include <basalt/server/runtime/world/MushroomGrowth.hpp>
#include <basalt/server/runtime/world/NetherPortalGrowth.hpp>
#include <basalt/server/runtime/world/RandomTicks.hpp>
#include <basalt/server/runtime/world/SaplingGrowth.hpp>
#include <basalt/server/runtime/world/StemGrowth.hpp>
#include <basalt/server/runtime/world/VineGrowth.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {
namespace {

constexpr std::array<std::string_view, 5> kCrops{"wheat", "carrots", "potatoes", "beetroots", "torchflower_crop"};

constexpr std::array<std::string_view, 2> kSlowCrops{"beetroots", "torchflower_crop"};

/// parity: the four `GrowingPlantHeadBlock` subclasses and their own odds.
constexpr std::array<std::string_view, 4> kClimbingPlants{"kelp", "cave_vines", "twisting_vines", "weeping_vines"};
constexpr double kKelpChance = 0.14;
constexpr double kVineChance = 0.1;

constexpr std::int32_t kCropLight = 9;

/// parity: `PitcherCropBlock.MAX_AGE` and its `DOUBLE_PLANT_AGE_INTERSECTION`.
constexpr std::int32_t kPitcherMaxAge = 4;
constexpr std::int32_t kPitcherDoubleAge = 3;

/// parity: `CropBlock.hasSufficientLight`, eight and not the nine a crop grows by.
constexpr std::int32_t kSurvivalLight = 8;

/// parity: `BeetrootBlock.randomTick`'s `nextInt(3) != 0`, and torchflower's own.
constexpr std::int32_t kSlowCropOdds = 3;
constexpr std::int32_t kNetherWartMaxAge = 3;
constexpr std::int32_t kNetherWartOdds = 10;
constexpr std::int32_t kStalkMaxAge = 15;
constexpr std::int32_t kMaxStalkHeight = 3;

/// `11 - state.getLightDampening()` for ice, and a plain 11 for a snow layer, which dampens none.
constexpr std::int32_t kMeltLight = 11;

/// parity: `LeavesBlock.DECAY_DISTANCE`.
constexpr std::int32_t kDecayDistance = 7;

constexpr std::int32_t kGrassLight = 9;
constexpr std::int32_t kGrassAttempts = 4;
constexpr std::int32_t kMaxMoisture = 7;

/// parity: `FarmlandBlock.isNearWater`'s own four.
constexpr std::int32_t kWaterRange = 4;

/// parity: `CactusBlock`, which flowers at age eight and more often once it is tall.
constexpr std::int32_t kFlowerAge = 8;
constexpr double kShortFlowerOdds = 0.1;
constexpr double kTallFlowerOdds = 0.25;

/// parity: how far a crop's speed reaches for farmland, one row on each side.
constexpr std::int32_t kFarmlandReach = 1;

[[nodiscard]] std::int32_t airState() { return registry::blocks::air().stateId; }

[[nodiscard]] std::int32_t dirtState() {
    const registry::BlockState* dirt = registry::blocks::named("dirt");
    return dirt == nullptr ? airState() : dirt->stateId;
}

[[nodiscard]] std::int32_t stateAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return world.blockState(x, y, z).value_or(airState());
}

[[nodiscard]] std::string_view pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string_view{} : block->path();
}

[[nodiscard]] std::optional<std::int32_t> numberOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
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

[[nodiscard]] std::optional<std::string> propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::nullopt : std::optional{found->second};
}

[[nodiscard]] bool isFading(std::int32_t from, std::int32_t to) {
    const std::string_view before = pathOf(from);
    const std::string_view after = pathOf(to);
    if (before.empty() || after.empty()) {
        return false;
    }
    // parity: `SpreadingSnowyDirtBlock.randomTick` turns a covered grass block or mycelium back to dirt.
    const bool dies = (before == "grass_block" || before == "mycelium") && after == "dirt";
    const bool taken =
        before.ends_with("ice") || before.ends_with("snow") || before == "fire" || after == "air" || dies;
    return before != after && taken;
}

void set(const randomTicks::Context& context, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
         std::vector<BlockChange>& changes) {
    const std::int32_t from = stateAt(context.world, x, y, z);
    const std::string_view cause = stateId == airState() || isFading(from, stateId) ? "fade" : "grow";
    if (context.mayChange && !context.mayChange(context.worldName, x, y, z, from, stateId, cause)) {
        return;
    }
    if (!context.world.setBlockState(x, y, z, stateId)) {
        return;
    }
    changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = stateId});
}

/// parity: `CropBlock.randomTick`.
void growCrop(const randomTicks::Context& context, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
              const wire::common::Identifier& block, random::RandomSource& random, std::vector<BlockChange>& changes) {
    // parity: `BeetrootBlock.randomTick` and `TorchflowerCropBlock.randomTick`.
    if (std::ranges::contains(kSlowCrops, block.path()) && random.nextInt(kSlowCropOdds) == 0) {
        return;
    }
    if (worldLight::rawBrightness(context.world, x, y, z, context.hasSkylight) < kCropLight) {
        return;
    }
    const registry::blockStateDefinitions::Definition* definition = registry::blockStateDefinitions::of(block);
    if (definition == nullptr) {
        return;
    }
    const std::size_t ages = definition->valuesOf("age").size();
    const std::optional<std::int32_t> age = numberOf(stateId, "age");
    if (!age.has_value() || ages == 0 || *age >= static_cast<std::int32_t>(ages) - 1) {
        return;
    }
    const float speed = randomTicks::growthSpeed(context.world, x, y, z, block);
    if (random.nextInt(static_cast<std::int32_t>(25.0F / speed) + 1) != 0) {
        return;
    }
    const std::optional<std::int32_t> grown =
        registry::blockStateDefinitions::stateIdOf(block, {{"age", std::to_string(*age + 1)}}, stateId);
    if (grown.has_value()) {
        set(context, x, y, z, *grown, changes);
    }
}

/// parity: `PitcherCropBlock.randomTick` and its `grow`.
void growPitcher(const randomTicks::Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                 std::int32_t stateId, const wire::common::Identifier& block, random::RandomSource& random,
                 std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> age = numberOf(stateId, "age");
    if (!age.has_value()) {
        return;
    }
    const float speed = randomTicks::growthSpeed(context.world, x, y, z, block);
    if (random.nextInt(static_cast<std::int32_t>(25.0F / speed) + 1) != 0) {
        return;
    }
    const std::int32_t grown = std::min(*age + 1, kPitcherMaxAge);
    // parity: `PitcherCropBlock.canGrow`.
    if (*age >= kPitcherMaxAge) {
        return;
    }
    if (worldLight::rawBrightness(context.world, x, y, z, context.hasSkylight) < kSurvivalLight) {
        return;
    }
    if (y + 1 > context.maxY) {
        return;
    }
    const bool doubled = grown >= kPitcherDoubleAge;
    if (doubled) {
        const std::optional<std::int32_t> above = context.world.blockState(x, y + 1, z);
        if (!above.has_value()) {
            return;
        }
        if (*above != airState() && pathOf(*above) != "pitcher_crop") {
            return;
        }
    }
    const std::optional<std::int32_t> lower =
        registry::blockStateDefinitions::stateIdOf(block, {{"age", std::to_string(grown)}, {"half", "lower"}}, stateId);
    if (!lower.has_value()) {
        return;
    }
    set(context, x, y, z, *lower, changes);
    if (!doubled) {
        return;
    }
    const std::optional<std::int32_t> upper =
        registry::blockStateDefinitions::stateIdOf(block, {{"age", std::to_string(grown)}, {"half", "upper"}}, stateId);
    if (upper.has_value()) {
        set(context, x, y + 1, z, *upper, changes);
    }
}

/// parity: `NetherWartBlock.randomTick`.
void growNetherWart(const randomTicks::Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                    std::int32_t stateId, const wire::common::Identifier& block, random::RandomSource& random,
                    std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> age = numberOf(stateId, "age");
    if (!age.has_value() || *age >= kNetherWartMaxAge || random.nextInt(kNetherWartOdds) != 0) {
        return;
    }
    const std::optional<std::int32_t> grown =
        registry::blockStateDefinitions::stateIdOf(block, {{"age", std::to_string(*age + 1)}}, stateId);
    if (grown.has_value()) {
        set(context, x, y, z, *grown, changes);
    }
}

/// parity: `SugarCaneBlock.randomTick`.
void growStalk(const randomTicks::Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
               std::int32_t stateId, const wire::common::Identifier& block, std::vector<BlockChange>& changes) {
    if (stateAt(context.world, x, y + 1, z) != airState()) {
        return;
    }
    std::int32_t height = 1;
    while (pathOf(stateAt(context.world, x, y - height, z)) == block.path()) {
        height++;
    }
    if (height >= kMaxStalkHeight) {
        return;
    }
    const std::optional<std::int32_t> age = numberOf(stateId, "age");
    if (!age.has_value()) {
        return;
    }
    if (*age < kStalkMaxAge) {
        const std::optional<std::int32_t> older =
            registry::blockStateDefinitions::stateIdOf(block, {{"age", std::to_string(*age + 1)}}, stateId);
        if (older.has_value()) {
            set(context, x, y, z, *older, changes);
        }
        return;
    }
    const std::optional<std::int32_t> fresh =
        registry::blockStateDefinitions::stateIdOf(block, {{"age", "0"}}, stateId);
    if (!fresh.has_value()) {
        return;
    }
    set(context, x, y + 1, z, *fresh, changes);
    set(context, x, y, z, *fresh, changes);
}

/// parity: `SaplingBlock.randomTick`: light nine above it, then one roll in seven.
void growSapling(const randomTicks::Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                 std::int32_t stateId, random::RandomSource& random, std::vector<BlockChange>& changes) {
    if (worldLight::rawBrightness(context.world, x, y + 1, z, context.hasSkylight) < saplingGrowth::kGrowthLight) {
        return;
    }
    if (random.nextInt(saplingGrowth::kGrowthOdds) != 0) {
        return;
    }
    const std::vector<BlockChange> grown =
        saplingGrowth::advance(context.world, x, y, z, stateId, random, context.flowering,
                               [&] { return !context.structureAllowed || context.structureAllowed(x, y, z, stateId); });
    changes.insert(changes.end(), grown.begin(), grown.end());
}

/// parity: `CactusBlock.randomTick`.
void growCactus(const randomTicks::Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                std::int32_t stateId, const wire::common::Identifier& block, random::RandomSource& random,
                std::vector<BlockChange>& changes) {
    if (stateAt(context.world, x, y + 1, z) != airState()) {
        return;
    }
    const std::optional<std::int32_t> age = numberOf(stateId, "age");
    if (!age.has_value()) {
        return;
    }
    std::int32_t height = 1;
    while (pathOf(stateAt(context.world, x, y - height, z)) == block.path()) {
        height++;
        if (height == kMaxStalkHeight && *age == kStalkMaxAge) {
            return;
        }
    }
    const bool supported = blockSurvival::isSupported(context.world, x, y + 1, z, stateId, {}, context.hasSkylight);
    if (*age == kFlowerAge && supported) {
        const double odds = height >= kMaxStalkHeight ? kTallFlowerOdds : kShortFlowerOdds;
        if (random.nextDouble() <= odds) {
            const registry::BlockState* flower = registry::blocks::named("cactus_flower");
            if (flower != nullptr) {
                set(context, x, y + 1, z, flower->stateId, changes);
            }
        }
    } else if (*age == kStalkMaxAge && height < kMaxStalkHeight) {
        const std::optional<std::int32_t> fresh =
            registry::blockStateDefinitions::stateIdOf(block, {{"age", "0"}}, stateId);
        if (!fresh.has_value()) {
            return;
        }
        set(context, x, y + 1, z, *fresh, changes);
        set(context, x, y, z, *fresh, changes);
        return;
    }
    if (*age < kStalkMaxAge) {
        const std::optional<std::int32_t> older =
            registry::blockStateDefinitions::stateIdOf(block, {{"age", std::to_string(*age + 1)}}, stateId);
        if (older.has_value()) {
            set(context, x, y, z, *older, changes);
        }
    }
}

/// parity: `SpreadingSnowyBlock.canStayAlive`.
[[nodiscard]] bool staysGrass(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::int32_t above = stateAt(world, x, y + 1, z);
    if (pathOf(above) == "snow" && propertyOf(above, "layers") == "1") {
        return true;
    }
    if (registry::blockSupport::holdsWater(above)) {
        return false;
    }
    return registry::blockShapes::dampening(above) < worldLight::kMaxLevel;
}

/// parity: `SpreadingSnowyBlock.randomTick`.
void spreadGrass(const randomTicks::Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                 std::int32_t stateId, random::RandomSource& random, std::vector<BlockChange>& changes) {
    if (!staysGrass(context.world, x, y, z)) {
        set(context, x, y, z, dirtState(), changes);
        return;
    }
    if (worldLight::rawBrightness(context.world, x, y + 1, z, context.hasSkylight) < kGrassLight) {
        return;
    }
    for (std::int32_t attempt = 0; attempt < kGrassAttempts; attempt++) {
        const std::int32_t tx = x + random.nextInt(3) - 1;
        const std::int32_t ty = y + random.nextInt(5) - 3;
        const std::int32_t tz = z + random.nextInt(3) - 1;
        // parity: both spread onto plain dirt, which is `SpreadingSnowyBlock`'s base block.
        if (stateAt(context.world, tx, ty, tz) != dirtState()) {
            continue;
        }
        if (!staysGrass(context.world, tx, ty, tz)) {
            continue;
        }
        if (registry::blockSupport::holdsWater(stateAt(context.world, tx, ty + 1, tz))) {
            continue;
        }
        set(context, tx, ty, tz, stateId, changes);
    }
}

/// parity: `FarmlandBlock.isNearWater`, four blocks out and one up.
[[nodiscard]] bool nearWater(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    for (std::int32_t dx = -kWaterRange; dx <= kWaterRange; dx++) {
        for (std::int32_t dz = -kWaterRange; dz <= kWaterRange; dz++) {
            for (std::int32_t dy = 0; dy <= 1; dy++) {
                if (registry::blockSupport::holdsWater(stateAt(world, x + dx, y + dy, z + dz))) {
                    return true;
                }
            }
        }
    }
    return false;
}

[[nodiscard]] bool moistureAllowed(const randomTicks::Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                   std::int32_t moisture) {
    return context.world.events().allowed(&server::plugins::pluginEventTypes::blockMoistureChange,
                                          [&](server::plugins::PluginEvent& event) {
                                              event.set("world", std::string{context.worldName});
                                              event.set("x", static_cast<std::int64_t>(x));
                                              event.set("y", static_cast<std::int64_t>(y));
                                              event.set("z", static_cast<std::int64_t>(z));
                                              event.set("moisture", static_cast<std::int64_t>(moisture));
                                          });
}

/// parity: `FarmlandBlock.randomTick`.
void dryFarmland(const randomTicks::Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                 std::int32_t stateId, std::vector<BlockChange>& changes) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    const std::optional<std::int32_t> moisture = numberOf(stateId, "moisture");
    if (block == nullptr || !moisture.has_value()) {
        return;
    }
    if (nearWater(context.world, x, y, z) || context.raining) {
        if (*moisture >= kMaxMoisture) {
            return;
        }
        const std::optional<std::int32_t> wet =
            registry::blockStateDefinitions::stateIdOf(*block, {{"moisture", std::to_string(kMaxMoisture)}}, stateId);
        if (wet.has_value() && moistureAllowed(context, x, y, z, kMaxMoisture)) {
            set(context, x, y, z, *wet, changes);
        }
        return;
    }
    if (*moisture > 0) {
        const std::optional<std::int32_t> drier =
            registry::blockStateDefinitions::stateIdOf(*block, {{"moisture", std::to_string(*moisture - 1)}}, stateId);
        if (drier.has_value() && moistureAllowed(context, x, y, z, *moisture - 1)) {
            set(context, x, y, z, *drier, changes);
        }
        return;
    }
    if (registry::blockConnections::maintainsFarmland(stateAt(context.world, x, y + 1, z))) {
        return;
    }
    set(context, x, y, z, dirtState(), changes);
}

/// parity: `LeavesBlock.randomTick`.
void decay(const randomTicks::Context& context, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
           std::vector<BlockChange>& changes) {
    // parity: `LeavesBlock.decaying`.
    if (propertyOf(stateId, "persistent") == "true") {
        return;
    }
    if (numberOf(stateId, "distance").value_or(kDecayDistance) < kDecayDistance) {
        return;
    }
    set(context, x, y, z, airState(), changes);
}

/// parity: `IceBlock` and `SnowLayerBlock`.
void melt(const randomTicks::Context& context, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
          std::int32_t becomes, std::vector<BlockChange>& changes) {
    if (worldLight::blockLight(context.world, x, y, z) <= kMeltLight - registry::blockShapes::dampening(stateId)) {
        return;
    }
    set(context, x, y, z, becomes, changes);
}

void actOnce(const randomTicks::Context& context, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
             random::RandomSource& random, std::vector<BlockChange>& changes) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return;
    }
    const std::string_view path = block->path();
    if (std::ranges::contains(kCrops, path)) {
        growCrop(context, x, y, z, stateId, *block, random, changes);
    } else if (path == "pitcher_crop") {
        growPitcher(context, x, y, z, stateId, *block, random, changes);
    } else if (path == "nether_wart") {
        growNetherWart(context, x, y, z, stateId, *block, random, changes);
    } else if (path == "sugar_cane") {
        growStalk(context, x, y, z, stateId, *block, changes);
    } else if (path == "cactus") {
        growCactus(context, x, y, z, stateId, *block, random, changes);
    } else if (registry::blockConnections::isLeaves(stateId)) {
        decay(context, x, y, z, stateId, changes);
    } else if (saplingGrowth::isSapling(stateId)) {
        growSapling(context, x, y, z, stateId, random, changes);
        // parity: `SpreadingSnowyBlock`.
    } else if (path == "grass_block" || path == "mycelium") {
        spreadGrass(context, x, y, z, stateId, random, changes);
    } else if (path == "farmland") {
        dryFarmland(context, x, y, z, stateId, changes);
    } else if (path == "crimson_nylium" || path == "warped_nylium") {
        miscGrowth::nylium(context.world, x, y, z, changes);
    } else if (path == "redstone_ore" || path == "deepslate_redstone_ore") {
        miscGrowth::redstoneOre(context.world, x, y, z, stateId, changes);
    } else if (path == "cocoa") {
        cocoaGrowth::cocoa(context.world, x, y, z, stateId, random, changes);
    } else if (path == "sweet_berry_bush") {
        miscGrowth::sweetBerries(context.world, x, y, z, stateId, random, changes, context.hasSkylight);
    } else if (path == "brown_mushroom" || path == "red_mushroom") {
        mushroomGrowth::mushroom(context.world, x, y, z, stateId, random, changes, {}, context.hasSkylight);
    } else if (path == "pumpkin_stem" || path == "melon_stem") {
        stemGrowth::stem(context.world, x, y, z, stateId, randomTicks::growthSpeed(context.world, x, y, z, *block),
                         random, changes, context.hasSkylight);
    } else if (path == "bamboo") {
        bambooGrowth::bamboo(context.world, x, y, z, stateId, random, changes, context.hasSkylight);
    } else if (path == "bamboo_sapling") {
        bambooGrowth::bambooSapling(context.world, x, y, z, random, changes);
    } else if (path == "nether_portal") {
        if (context.allocateEntityId) {
            (void)netherPortalGrowth::netherPortal(context.world, context.gameRules, context.difficulty,
                                                   context.spawnsPortalPiglins, x, y, z, random,
                                                   context.allocateEntityId);
        }
    } else if (path.ends_with("eyeblossom")) {
        miscGrowth::eyeblossom(context.world, context.clock, x, y, z, stateId, random, changes);
    } else if (path == "pointed_dripstone" || path == "sulfur_spike") {
        dripstoneGrowth::dripstone(context.world, x, y, z, stateId, random, changes, context.evaporates,
                                   context.levelEvent, &context.clock, context.pushPlayersUp);
    } else if (path == "vine") {
        vineGrowth::vine(context.world, context.gameRules, x, y, z, stateId, random, changes);
    } else if (path == "dried_ghast") {
        miscGrowth::driedGhast(context.world, x, y, z, stateId);
    } else if (path == "chorus_flower") {
        chorusFlowerGrowth::chorusFlower(context.world, x, y, z, stateId, random, changes);
    } else if (path == "budding_amethyst") {
        miscGrowth::buddingAmethyst(context.world, x, y, z, random, changes);
    } else if (path == "turtle_egg") {
        miscGrowth::turtleEgg(context.world, x, y, z, stateId, random, changes);
    } else if (path == "lava") {
        lavaGrowth::lava(context.world, x, y, z, random, changes);
    } else if (std::ranges::contains(kClimbingPlants, path)) {
        const bool upwards = path == "kelp" || path == "twisting_vines";
        const double chance = path == "kelp" ? kKelpChance : kVineChance;
        climbingPlantGrowth::growingPlant(context.world, x, y, z, stateId, upwards, chance, random, changes);
    } else if (registry::weathering::weathers(path)) {
        weatheringGrowth::weather(context.world, x, y, z, stateId, random, changes);
    } else if (path == "ice") {
        // parity: `IceBlock.melt`, which removes the ice rather than watering a dimension that.
        melt(context, x, y, z, stateId, context.evaporates ? airState() : registry::blocks::water().stateId, changes);
    } else if (path == "snow") {
        melt(context, x, y, z, stateId, airState(), changes);
    }
}

void act(const randomTicks::Context& context, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
         random::RandomSource& random, std::vector<BlockChange>& changes) {
    const wire::common::Identifier* block =
        context.growth == nullptr ? nullptr : registry::blockStateDefinitions::blockOf(stateId);
    const std::int32_t percent = block == nullptr ? 100 : context.growth->forBlock(block->path());
    if (percent == 100) {
        actOnce(context, x, y, z, stateId, random, changes);
        return;
    }
    // Not vanilla: a percent of vanilla's growth attempts, the whole part always and the rest by chance.
    const std::int32_t attempts = percent / 100 + (random.nextInt(100) < percent % 100 ? 1 : 0);
    for (std::int32_t attempt = 0; attempt < attempts; attempt++) {
        const std::int32_t current = attempt == 0 ? stateId : stateAt(context.world, x, y, z);
        if (registry::blockStateDefinitions::blockOf(current) != block) {
            break;
        }
        actOnce(context, x, y, z, current, random, changes);
    }
}

} // namespace

float randomTicks::growthSpeed(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                               const wire::common::Identifier& block) {
    float speed = 1.0F;
    for (std::int32_t dx = -kFarmlandReach; dx <= kFarmlandReach; dx++) {
        for (std::int32_t dz = -kFarmlandReach; dz <= kFarmlandReach; dz++) {
            const std::optional<std::int32_t> under = world.blockState(x + dx, y - 1, z + dz);
            if (!under.has_value()) {
                continue;
            }
            float here = 0.0F;
            if (pathOf(*under) == "farmland") {
                here = 1.0F;
                if (numberOf(*under, "moisture").value_or(0) > 0) {
                    here = 3.0F;
                }
            }
            if (dx != 0 || dz != 0) {
                here /= 4.0F;
            }
            speed += here;
        }
    }
    const auto sameAt = [&world, y, &block](std::int32_t ax, std::int32_t az) {
        return pathOf(stateAt(world, ax, y, az)) == block.path();
    };
    const bool horizontal = sameAt(x - 1, z) || sameAt(x + 1, z);
    const bool vertical = sameAt(x, z - 1) || sameAt(x, z + 1);
    if (horizontal && vertical) {
        return speed / 2.0F;
    }
    const bool diagonal = sameAt(x - 1, z - 1) || sameAt(x + 1, z - 1) || sameAt(x + 1, z + 1) || sameAt(x - 1, z + 1);
    return diagonal ? speed / 2.0F : speed;
}

std::int32_t RandomTicks::nextWalk(std::int32_t& state) {
    // Wrapping is the point: vanilla's is a Java int and this walk depends on it overflowing.
    state = static_cast<std::int32_t>(
        (static_cast<std::uint32_t>(state) * static_cast<std::uint32_t>(randomTicks::kWalkMultiplier)) +
        static_cast<std::uint32_t>(randomTicks::kWalkAddend));
    return state >> 2;
}

std::vector<BlockChange> RandomTicks::tick(const randomTicks::Context& context,
                                           std::span<const std::array<std::int32_t, 2>> chunks, std::int32_t speed,
                                           random::RandomSource& random) {
    if (speed <= 0) {
        return {};
    }
    std::vector<BlockChange> changes;
    const auto found = walks_.find(context.worldName);
    std::int32_t walkState = found == walks_.end() ? static_cast<std::int32_t>(context.seed) : found->second;
    for (const std::array<std::int32_t, 2>& position : chunks) {
        const server::world::Chunk* chunk = context.world.chunkIfLoaded(position[0], position[1]);
        if (chunk == nullptr) {
            continue;
        }
        // parity: a chunk ticks once it is lit, which the light thread gets to a moment after it stands.
        if (context.world.lightsAsynchronously() &&
            (chunk->cachedBlockLight() == nullptr || (context.hasSkylight && chunk->cachedSkyLight() == nullptr))) {
            context.world.requestLight(position[0], position[1]);
            continue;
        }
        const std::int32_t minX = position[0] << 4;
        const std::int32_t minZ = position[1] << 4;
        for (std::size_t index = 0; index < chunk->sectionCount(); index++) {
            // parity: `LevelChunkSection.isRandomlyTicking`. Skipping the sections that hold nothing.
            if (!chunk->section(index).mayContain(registry::blockSupport::ticksRandomly)) {
                continue;
            }
            const std::int32_t sectionY = chunk->verticalBounds().minY() + (static_cast<std::int32_t>(index) << 4);
            for (std::int32_t step = 0; step < speed; step++) {
                const std::int32_t walk = nextWalk(walkState);
                const std::int32_t x = minX + (walk & 15);
                const std::int32_t y = sectionY + ((walk >> 16) & 15);
                const std::int32_t z = minZ + ((walk >> 8) & 15);
                const std::int32_t stateId = chunk->blockStateId(x & 15, y, z & 15);
                if (registry::blockSupport::ticksRandomly(stateId)) {
                    act(context, x, y, z, stateId, random, changes);
                }
            }
        }
    }
    walks_.insert_or_assign(std::string{context.worldName}, walkState);
    return changes;
}

std::vector<BlockChange> RandomTicks::tickBlock(const randomTicks::Context& context, std::int32_t x, std::int32_t y,
                                                std::int32_t z, random::RandomSource& random) {
    const std::optional<std::int32_t> stateId = context.world.blockState(x, y, z);
    if (!stateId.has_value() || !registry::blockSupport::ticksRandomly(*stateId)) {
        return {};
    }
    std::vector<BlockChange> changes;
    act(context, x, y, z, *stateId, random, changes);
    return changes;
}

} // namespace basalt::server::runtime::world
