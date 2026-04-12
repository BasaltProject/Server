#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Attributes.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/Creakings.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerParticles.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::protocol::play::Gamemode;
namespace creakings = entity::creakings;
namespace kit = entity::ai::brainKit;
namespace mth = wire::common::mth;

/// parity: `Creaking`'s follow range, which is how far its player sensor reaches.
constexpr double kFollowRange = 32.0;
/// parity: `Creaking.ACTIVATION_RANGE_SQ`.
constexpr double kActivationRangeSquared = 144.0;
/// parity: `EntityType.CREAKING`'s eyes.
constexpr float kEyeHeight = 2.3F;
/// parity: `LivingEntity.hasLineOfSight`, which gives up past this.
constexpr double kSightRange = 128.0;

/// One player the creaking's sensor sees.
struct Nearby {
    PlayerSession* player = nullptr;
    double distanceSquared = 0.0;
};

/// parity: `Player`'s eyes, standing or crouched, as floats.
[[nodiscard]] double eyeOf(const PlayerSession& player) {
    return player.y + static_cast<double>(player.sneaking ? 1.27F : 1.62F);
}

/// parity: `LivingEntity.getScale`.
[[nodiscard]] float scaleOf(const ServerEntity& creaking) {
    return static_cast<float>(creaking.attributeWithModifiers(entity::attribute::kScale));
}

[[nodiscard]] double creakingEye(const ServerEntity& creaking) {
    return creaking.y + static_cast<double>(kEyeHeight * scaleOf(creaking));
}

/// parity: `LivingEntity.hasLineOfSight(target, block, NONE, eyeY)`: nothing past 128 blocks, else a clip.
[[nodiscard]] bool hasLineOfSight(world::WorldState& world, double fromX, double fromY, double fromZ, double toX,
                                  double toY, double toZ, entity::blockRay::ClipShape shape,
                                  const entity::blockRay::ClipViewer& viewer) {
    const double dx = fromX - toX;
    const double dy = fromY - toY;
    const double dz = fromZ - toZ;
    if (std::sqrt((dx * dx) + (dy * dy) + (dz * dz)) > kSightRange) {
        return false;
    }
    return !entity::blockRay::clips(world, fromX, fromY, fromZ, toX, toY, toZ, shape, viewer);
}

/// parity: `Vec3.normalize`, nothing at all under a float 1.0E-5.
[[nodiscard]] std::array<double, 3> normalized(double x, double y, double z) {
    const double length = std::sqrt((x * x) + (y * y) + (z * z));
    if (length < static_cast<double>(1.0E-5F)) {
        return {0.0, 0.0, 0.0};
    }
    return {x / length, y / length, z / length};
}

/// parity: `LivingEntity.isLookingAtMe(player, 0.5, false, true, heights)`: the player's view within the
/// cone of one of the three heights, and nothing but see-through blocks in between.
[[nodiscard]] bool isLookingAt(world::WorldState& world, const PlayerSession& player, const ServerEntity& creaking) {
    // parity: `Entity.calculateViewVector`, normalised.
    constexpr float kRadians = std::numbers::pi_v<float> / 180.0F;
    const float pitch = player.pitch * kRadians;
    const float yaw = -player.yaw * kRadians;
    const float yawCos = mth::cos(yaw);
    const float yawSin = mth::sin(yaw);
    const float pitchCos = mth::cos(pitch);
    const float pitchSin = mth::sin(pitch);
    const std::array<double, 3> look = normalized(
        static_cast<double>(yawSin * pitchCos), static_cast<double>(-pitchSin), static_cast<double>(yawCos * pitchCos));
    const double eye = creakingEye(creaking);
    const entity::blockRay::ClipViewer viewer{
        .feetY = player.y, .descending = player.sneaking, .fallDistance = player.fallDistance};
    const std::array<double, 3> gazes{eye, creaking.y + (0.5 * static_cast<double>(scaleOf(creaking))),
                                      (eye + creaking.y) / 2.0};
    for (const double gaze : gazes) {
        const std::array<double, 3> towards =
            normalized(creaking.x - player.x, gaze - eyeOf(player), creaking.z - player.z);
        const double dot = (look[0] * towards[0]) + (look[1] * towards[1]) + (look[2] * towards[2]);
        if (dot > 1.0 - (0.5 / 1.0) && hasLineOfSight(world, player.x, eyeOf(player), player.z, creaking.x, gaze,
                                                      creaking.z, entity::blockRay::ClipShape::Visual, viewer)) {
            return true;
        }
    }
    return false;
}

} // namespace

void MobActionSystem::tearDownCreaking(world::LoadedWorld& world, ServerEntity& creaking) {
    // parity: `Creaking.tearDown`: splinters of pale oak and of the heart, its death sound, and gone.
    const double halfWidth = creaking.width() / 2.0;
    const double centerY = creaking.y + (creaking.height() / 2.0);
    const auto spread = [&](std::int32_t stateId, std::int32_t count) {
        const wire::protocol::play::LevelParticlesPacket packet{
            .particleTypeId = ExplosionSystem::particleId("block_crumble"),
            .x = creaking.x,
            .y = centerY,
            .z = creaking.z,
            .offsetX = static_cast<float>(halfWidth * 2.0 * 0.3),
            .offsetY = static_cast<float>(creaking.height() * 0.3),
            .offsetZ = static_cast<float>(halfWidth * 2.0 * 0.3),
            .count = count,
            .options = wire::protocol::play::ParticleOptions{
                wire::protocol::play::ParticleOptions::BlockState{.stateId = stateId}}};
        for (const PlayerSession* watcher : watchersOf(world)) {
            player::showParticleFor(*watcher, "minecraft:block_crumble", packet);
        }
    };
    if (const std::optional<std::int32_t> wood =
            server::registry::blocks::defaultStateIdOf(wire::common::Identifier::vanilla("pale_oak_wood"));
        wood.has_value()) {
        spread(*wood, 100);
    }
    const wire::common::Identifier heartBlock = wire::common::Identifier::vanilla("creaking_heart");
    if (const std::optional<std::int32_t> heart = server::registry::blockStateDefinitions::stateIdOf(
            heartBlock, {{"creaking_heart_state", "awake"}}, server::registry::blocks::defaultStateIdOf(heartBlock));
        heart.has_value()) {
        spread(*heart, 10);
    }
    makeSound(world, creaking, "entity.creaking.death", 1.0F);
    creaking.removed = true;
}

void MobActionSystem::tickCreaking(world::LoadedWorld& world, entity::ai::MobContext& context) {
    ServerEntity& creaking = context.mob;
    creakings::CreakingState& state = creakings::stateOf(creaking);
    const std::int64_t now = context.gameTime;
    // parity: what its navigation reads of the level: `getGameTime` and the difficulty's id.
    state.gameTime = now;
    const std::string& difficulty = world.difficulty();
    state.difficulty = difficulty == "peaceful" ? 0 : difficulty == "easy" ? 1 : difficulty == "hard" ? 3 : 2;
    // parity: `Creaking.tick`: one bound to a heart that no longer claims it goes; a heart broken
    // out from under it tore it down.
    if (state.home.has_value()) {
        const std::optional<bool> claimed = heartClaims(world, *state.home, creaking);
        if (!claimed.has_value()) {
            tearDownCreaking(world, creaking);
            return;
        }
        if (!*claimed) {
            creaking.health = 0.0;
            creaking.dying = true;
            return;
        }
    }
    // parity: `hurtServer`'s shudder, `ENTITY_ACTION`, the heart's call and the sway.
    if (std::exchange(state.hurtPending, false)) {
        entityEvent(creaking, creakings::kEventInvulnerable);
        world::vibrations::emit(world.state(), "entity_action", creaking.x, creaking.y, creaking.z, now,
                                {.entity = creaking.uuid()});
        if (state.home.has_value()) {
            if (state.hurtByPlayer) {
                creakingHeartHurt(world, *state.home);
            }
            makeSound(world, creaking, "entity.creaking.sway", 1.0F);
        }
    }
    // parity: `Creaking.aiStep`.
    if (state.invulnerabilityTicks > 0) {
        --state.invulnerabilityTicks;
    }
    if (state.attackAnimationTicks > 0) {
        --state.attackAnimationTicks;
    }
    // parity: `NEAREST_PLAYERS`, everybody alive and not spectating inside the follow range.
    std::vector<Nearby> players;
    for (PlayerSession* player : watchersOf(world)) {
        const double dx = player->x - creaking.x;
        const double dy = player->y - creaking.y;
        const double dz = player->z - creaking.z;
        const double distance = (dx * dx) + (dy * dy) + (dz * dz);
        if (!player->dead && player->gamemode != Gamemode::Spectator && distance <= kFollowRange * kFollowRange) {
            players.push_back({player, distance});
        }
    }
    std::ranges::stable_sort(players, {}, &Nearby::distanceSquared);
    const auto attackable = [](const PlayerSession& player) { return player.gamemode != Gamemode::Creative; };
    const auto setActive = [&](bool active, std::optional<std::int32_t> target) {
        creaking.targetId = target;
        world::vibrations::emit(world.state(), "entity_action", creaking.x, creaking.y, creaking.z, now,
                                {.entity = creaking.uuid()});
        makeSound(world, creaking, active ? "entity.creaking.activate" : "entity.creaking.deactivate", 1.0F);
        state.active = active;
    };
    // parity: `checkCanMove`: frozen while somebody it could attack looks at it.
    const bool nowCanMove = [&] {
        if (players.empty()) {
            if (state.active) {
                setActive(false, std::nullopt);
            }
            return true;
        }
        bool potentialTarget = false;
        for (const Nearby& seen : players) {
            if (!attackable(*seen.player)) {
                continue;
            }
            potentialTarget = true;
            const std::optional<storage::StoredItemStack> head =
                seen.player->inventory.at(player::PlayerInventory::kArmourStart);
            const auto headItem = wire::common::Identifier::parse(head.has_value() ? head->id : std::string{});
            const bool disguised =
                head.has_value() && headItem.has_value() && options_.itemTags != nullptr &&
                world::realItemTags::contains(*options_.itemTags, "minecraft:gaze_disguise_equipment", *headItem);
            if ((!state.active || !disguised) && isLookingAt(world.state(), *seen.player, creaking)) {
                if (state.active) {
                    return false;
                }
                if (seen.distanceSquared < kActivationRangeSquared) {
                    setActive(true, seen.player->entityId());
                    return false;
                }
            }
        }
        if (!potentialTarget && state.active) {
            setActive(false, std::nullopt);
        }
        return true;
    }();
    if (nowCanMove != state.canMove) {
        world::vibrations::emit(world.state(), "entity_action", creaking.x, creaking.y, creaking.z, now,
                                {.entity = creaking.uuid()});
        if (nowCanMove) {
            makeSound(world, creaking, "entity.creaking.unfreeze", 1.0F);
        } else {
            // parity: `stopInPlace`, and `MoveToTargetSink` stops with the path it loses.
            entity::ai::navigation::stop(creaking);
            state.pathNodeTypes.clear();
            state.motion.walk.reset();
            creaking.aiSpeed = 0.0F;
            creaking.velocityX = 0.0;
            creaking.velocityY = 0.0;
            creaking.velocityZ = 0.0;
            makeSound(world, creaking, "entity.creaking.freeze", 1.0F);
        }
    }
    state.canMove = nowCanMove;
    // parity: `playerIsStuckInYou`, which its heart asks.
    state.playerStuckCounter = [&] {
        for (const Nearby& seen : players) {
            const double halfWidth = creaking.width() / 2.0;
            const double eye = eyeOf(*seen.player);
            if (std::abs(seen.player->x - creaking.x) < halfWidth &&
                std::abs(seen.player->z - creaking.z) < halfWidth && eye >= creaking.y &&
                eye < creaking.y + creaking.height()) {
                return state.playerStuckCounter + 1;
            }
        }
        return 0;
    }();
    // parity: `NEAREST_VISIBLE_ATTACKABLE_PLAYER(S)`, in sight of its eyes through what it collides with.
    std::vector<std::int32_t> visible;
    const entity::blockRay::ClipViewer ownView{.feetY = creaking.y, .fallDistance = creaking.fallDistance};
    for (const Nearby& seen : players) {
        if (attackable(*seen.player) &&
            hasLineOfSight(world.state(), creaking.x, creakingEye(creaking), creaking.z, seen.player->x,
                           eyeOf(*seen.player), seen.player->z, entity::blockRay::ClipShape::Collider, ownView)) {
            visible.push_back(seen.player->entityId());
        }
    }
    // parity: the core's `Swim(0.8)`, only while it may move.
    if (state.canMove && entity::ai::inWater(context.world, creaking) && context.random.nextFloat() < 0.8F) {
        creaking.velocityY += 0.04;
    }
    // parity: `updateActivity`: idle while frozen, else fight whenever it has a target.
    const bool fighting = state.canMove && creaking.targetId.has_value();
    if (!fighting) {
        // parity: `StartAttacking` for an active creaking.
        if (state.active && !creaking.targetId.has_value() && !visible.empty()) {
            creaking.targetId = visible.front();
        }
        kit::lookSometimes(context, state.look, state.lookTicker, 8.0, 30, 60);
        kit::idleMovement(context, state.motion, state.look, state.idleTicks, 0.3F);
    } else {
        // parity: `StopAttackingIfTargetInvalid`, a target out of its sight.
        if (std::ranges::find(visible, *creaking.targetId) == visible.end()) {
            creaking.targetId.reset();
        } else {
            kit::chaseTarget(context, state.motion, state.look, 1.0F);
            if (kit::meleeAttack(context, state.attackCoolingUntil, creakings::kAttackInterval)) {
                // parity: `Creaking.doHurtTarget`, the swing's event before the blow.
                state.attackAnimationTicks = 15;
                entityEvent(creaking, creakings::kEventAttack);
            }
        }
    }
    // parity: the core's `MoveToTargetSink` and `LookAtTargetSink` run frozen or not; it is the
    // creaking's own navigation, move and look controls that hold still.
    kit::moveToTarget(context, state.motion);
    kit::lookAtTarget(context, state.look);
}

} // namespace basalt::server::runtime
