#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>
#include <basalt/server/runtime/world/MiscGrowth.hpp>
#include <basalt/server/runtime/world/ScheduledBlocks.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::runtime::world::miscGrowth {
namespace {

constexpr std::int32_t kFullDampening = 15;
constexpr std::int32_t kBerryOdds = 5;
constexpr std::int32_t kBerryMaxAge = 3;
constexpr std::int32_t kAmethystOdds = 5;
constexpr std::int32_t kTurtleHatch = 2;

/// parity: `EnvironmentAttributes.TURTLE_EGG_HATCH_CHANCE`'s default.
constexpr float kTurtleHatchChance = 0.002F;

/// parity: `DriedGhastBlock.randomTick`'s own delay.
constexpr std::int32_t kDriedGhastDelay = 5000;

/// parity: `tryChangingState`'s `offset(-3.
constexpr std::int32_t kEyeblossomReach = 3;
constexpr std::int32_t kEyeblossomHeight = 2;
constexpr double kEyeblossomNear = 5.0;
constexpr double kEyeblossomFar = 10.0;

/// parity: the `day` timeline's keyframes for `gameplay/eyeblossom_open`.
constexpr std::int64_t kEyeblossomOpens = 12600;
constexpr std::int64_t kEyeblossomCloses = 23401;
constexpr std::int64_t kDayTicks = 24000;

[[nodiscard]] std::optional<std::string> propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::nullopt : std::optional{found->second};
}

[[nodiscard]] std::optional<std::int32_t> numberOf(const std::optional<std::string>& text) {
    if (!text.has_value()) {
        return std::nullopt;
    }
    std::int32_t value = 0;
    const std::from_chars_result result = std::from_chars(text->data(), text->data() + text->size(), value);
    if (result.ec != std::errc{} || result.ptr != text->data() + text->size()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::string_view pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string_view{} : block->path();
}

/// parity: the `day` timeline's two keyframes for this attribute.
[[nodiscard]] bool eyeblossomsAreOpen(const WorldClock& clock) {
    const std::int64_t time = ((clock.totalTicks() % kDayTicks) + kDayTicks) % kDayTicks;
    return time >= kEyeblossomOpens && time < kEyeblossomCloses;
}

} // namespace

void nylium(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> above = world.blockState(x, y + 1, z);
    if (!above.has_value() || registry::blockShapes::dampening(*above) < kFullDampening) {
        return;
    }
    growthSupport::set(world, x, y, z, registry::blocks::named("netherrack")->stateId, changes);
}

std::optional<BlockChange> lightRedstoneOre(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || (block->path() != "redstone_ore" && block->path() != "deepslate_redstone_ore")) {
        return std::nullopt;
    }
    if (propertyOf(*stateId, "lit") == std::optional<std::string>{"true"}) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> lit =
        registry::blockStateDefinitions::stateIdOf(*block, {{"lit", "true"}}, *stateId);
    if (!lit.has_value() || !world.setBlockState(x, y, z, *lit)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *lit};
}

void redstoneOre(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                 std::vector<BlockChange>& changes) {
    if (propertyOf(stateId, "lit") != std::optional<std::string>{"true"}) {
        return;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return;
    }
    const std::optional<std::int32_t> dark =
        registry::blockStateDefinitions::stateIdOf(*block, {{"lit", "false"}}, stateId);
    if (!dark.has_value()) {
        return;
    }
    growthSupport::set(world, x, y, z, *dark, changes);
}

void sweetBerries(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                  random::RandomSource& random, std::vector<BlockChange>& changes, bool hasSkylight) {
    if (random.nextInt(kBerryOdds) != 0) {
        return;
    }
    if (worldLight::rawBrightness(world, x, y + 1, z, hasSkylight) < growthSupport::kGrowthLight) {
        return;
    }
    growthSupport::grow(world, x, y, z, stateId, kBerryMaxAge, changes);
}

void buddingAmethyst(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                     std::vector<BlockChange>& changes) {
    if (random.nextInt(kAmethystOdds) != 0) {
        return;
    }
    const std::string facing{
        growthSupport::kFaceNames[static_cast<std::size_t>(random.nextInt(growthSupport::kFaceNames.size()))]};
    const std::int32_t targetX = x + growthSupport::offsetX(facing);
    const std::int32_t targetY = y + growthSupport::offsetY(facing);
    const std::int32_t targetZ = z + growthSupport::offsetZ(facing);
    const std::optional<std::int32_t> target = world.blockState(targetX, targetY, targetZ);
    if (!target.has_value()) {
        return;
    }
    const bool waterlogged = registry::blockSupport::holdsWater(*target);
    const std::string_view path = pathOf(*target);
    const std::optional<std::string> facingNow = propertyOf(*target, "facing");
    std::string_view next;
    if (*target == registry::blocks::air().stateId || waterlogged) {
        next = "small_amethyst_bud";
    } else if (facingNow != std::optional<std::string>{facing}) {
        return;
    } else if (path == "small_amethyst_bud") {
        next = "medium_amethyst_bud";
    } else if (path == "medium_amethyst_bud") {
        next = "large_amethyst_bud";
    } else if (path == "large_amethyst_bud") {
        next = "amethyst_cluster";
    } else {
        return;
    }
    const std::optional<std::int32_t> grown = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla(std::string{next}),
        {{"facing", facing}, {"waterlogged", waterlogged ? "true" : "false"}}, std::nullopt);
    if (!grown.has_value()) {
        return;
    }
    growthSupport::set(world, targetX, targetY, targetZ, *grown, changes);
}

void turtleEgg(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
               random::RandomSource& random, std::vector<BlockChange>& changes) {
    // parity: `TurtleEggBlock.shouldUpdateHatchLevel` over the `turtle_egg_hatch_chance` environment.
    if (random.nextFloat() >= kTurtleHatchChance) {
        return;
    }
    const std::string_view below = pathOf(world.blockState(x, y - 1, z).value_or(0));
    if (below != "sand" && below != "red_sand" && below != "suspicious_sand") {
        return;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return;
    }
    const std::optional<std::int32_t> hatch = numberOf(propertyOf(stateId, "hatch"));
    if (!hatch.has_value()) {
        return;
    }
    if (*hatch < kTurtleHatch) {
        const std::optional<std::int32_t> cracked =
            registry::blockStateDefinitions::stateIdOf(*block, {{"hatch", std::to_string(*hatch + 1)}}, stateId);
        if (!cracked.has_value()) {
            return;
        }
        growthSupport::set(world, x, y, z, *cracked, changes);
        return;
    }
    growthSupport::set(world, x, y, z, registry::blocks::air().stateId, changes);
}

void driedGhast(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    const std::int32_t hydration = numberOf(propertyOf(stateId, "hydration")).value_or(0);
    if (propertyOf(stateId, "waterlogged") != std::optional<std::string>{"true"} && hydration <= 0) {
        return;
    }
    // parity: `DriedGhastBlock.randomTick` is gated on `!hasScheduledTick(pos.
    if (world.hasScheduledBlockTick(x, y, z, scheduledBlocks::driedGhastTickType())) {
        return;
    }
    world.scheduleBlockTick(x, y, z, scheduledBlocks::driedGhastTickType(), kDriedGhastDelay);
}

bool isEyeblossom(const std::int32_t stateId) {
    const std::string_view path = pathOf(stateId);
    return path == "open_eyeblossom" || path == "closed_eyeblossom" || path == "potted_open_eyeblossom" ||
           path == "potted_closed_eyeblossom";
}

void eyeblossom(WorldState& world, const WorldClock& clock, std::int32_t x, std::int32_t y, std::int32_t z,
                std::int32_t stateId, random::RandomSource& random, std::vector<BlockChange>& changes) {
    const std::string_view path = pathOf(stateId);
    if (path.empty()) {
        return;
    }
    const bool open = path.starts_with("open_") || path.starts_with("potted_open_");
    if (open == eyeblossomsAreOpen(clock)) {
        return;
    }
    std::string_view other;
    if (path == "open_eyeblossom") {
        other = "closed_eyeblossom";
    } else if (path == "closed_eyeblossom") {
        other = "open_eyeblossom";
    } else if (path == "potted_open_eyeblossom") {
        other = "potted_closed_eyeblossom";
    } else if (path == "potted_closed_eyeblossom") {
        other = "potted_open_eyeblossom";
    } else {
        return;
    }
    const wire::common::Identifier target = wire::common::Identifier::vanilla(std::string{other});
    std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(
        target, registry::blockStateDefinitions::propertiesOf(stateId), std::nullopt);
    if (!turned.has_value()) {
        const registry::BlockState* plain = registry::blocks::named(std::string{other});
        if (plain == nullptr) {
            return;
        }
        turned = plain->stateId;
    }
    growthSupport::set(world, x, y, z, *turned, changes);
    const wire::common::Identifier* was = registry::blockStateDefinitions::blockOf(stateId);
    if (was == nullptr) {
        return;
    }
    // parity: `tryChangingState` books every flower in the same state inside a box of three across.
    for (std::int32_t dz = -kEyeblossomReach; dz <= kEyeblossomReach; ++dz) {
        for (std::int32_t dy = -kEyeblossomHeight; dy <= kEyeblossomHeight; ++dy) {
            for (std::int32_t dx = -kEyeblossomReach; dx <= kEyeblossomReach; ++dx) {
                if (world.blockState(x + dx, y + dy, z + dz) != std::optional<std::int32_t>{stateId}) {
                    continue;
                }
                const double distance = std::sqrt(static_cast<double>((dx * dx) + (dy * dy) + (dz * dz)));
                const std::int32_t delay =
                    random.nextIntBetweenInclusive(static_cast<std::int32_t>(distance * kEyeblossomNear),
                                                   static_cast<std::int32_t>(distance * kEyeblossomFar));
                world.scheduleBlockTick(x + dx, y + dy, z + dz, *was, delay);
            }
        }
    }
}

} // namespace basalt::server::runtime::world::miscGrowth
