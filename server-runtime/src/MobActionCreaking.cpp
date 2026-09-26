#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
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
#include <cmath>
#include <numbers>
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
constexpr double kEyeHeight = 2.3;

/// One player the creaking's sensor sees.
struct Nearby {
    PlayerSession* player = nullptr;
    double distanceSquared = 0.0;
};

[[nodiscard]] double eyeOf(const PlayerSession& player) { return player.y + world::fluids::eyeHeight(player.sneaking); }

/// parity: `LivingEntity.hasLineOfSight(target, block, fluid, eyeY)` from the player to the creaking.
[[nodiscard]] bool playerSees(world::WorldState& world, const PlayerSession& player, const ServerEntity& creaking,
                              double gazeY) {
    const double dx = creaking.x - player.x;
    const double dy = gazeY - eyeOf(player);
    const double dz = creaking.z - player.z;
    if (std::sqrt((dx * dx) + (dy * dy) + (dz * dz)) > 128.0) {
        return false;
    }
    return !entity::blockRay::cast(world, player.x, eyeOf(player), player.z, creaking.x, gazeY, creaking.z).has_value();
}

/// parity: `LivingEntity.isLookingAtMe(player, 0.5, false, true, heights)`.
[[nodiscard]] bool isLookingAt(world::WorldState& world, const PlayerSession& player, const ServerEntity& creaking) {
    constexpr float kRadians = std::numbers::pi_v<float> / 180.0F;
    const float pitch = player.pitch * kRadians;
    const float yaw = -player.yaw * kRadians;
    const double lookX = static_cast<double>(mth::sin(yaw) * mth::cos(pitch));
    const double lookY = static_cast<double>(-mth::sin(pitch));
    const double lookZ = static_cast<double>(mth::cos(yaw) * mth::cos(pitch));
    const double lookLength = std::sqrt((lookX * lookX) + (lookY * lookY) + (lookZ * lookZ));
    const double eye = creaking.y + kEyeHeight;
    for (const double gaze : {eye, creaking.y + 0.5, (eye + creaking.y) / 2.0}) {
        const double dx = creaking.x - player.x;
        const double dy = gaze - eyeOf(player);
        const double dz = creaking.z - player.z;
        const double length = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
        if (length < 1.0E-4 || lookLength < 1.0E-4) {
            continue;
        }
        const double dot = ((lookX * dx) + (lookY * dy) + (lookZ * dz)) / (lookLength * length);
        if (dot > 1.0 - 0.5 && playerSees(world, player, creaking, gaze)) {
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
            // parity: `stopInPlace`.
            entity::ai::navigation::stop(creaking);
            state.motion.walk.reset();
            creaking.velocityX = 0.0;
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
    // parity: `NEAREST_VISIBLE_ATTACKABLE_PLAYER(S)`, in sight of its eyes.
    std::vector<std::int32_t> visible;
    for (const Nearby& seen : players) {
        if (attackable(*seen.player) &&
            !entity::blockRay::cast(world.state(), creaking.x, creaking.y + kEyeHeight, creaking.z, seen.player->x,
                                    eyeOf(*seen.player), seen.player->z)
                 .has_value()) {
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
    // parity: `CreakingMoveControl`, `CreakingLookControl` and the navigation, all still while frozen.
    if (state.canMove) {
        kit::moveToTarget(context, state.motion);
        kit::lookAtTarget(context, state.look);
    } else {
        entity::ai::navigation::stop(creaking);
    }
}

} // namespace basalt::server::runtime
