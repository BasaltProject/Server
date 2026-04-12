#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Armadillos.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using wire::protocol::play::SoundSource;
namespace armadillos = entity::armadillos;
namespace kit = entity::ai::brainKit;
namespace mth = wire::common::mth;
using armadillos::ArmadilloState;
using armadillos::State;

constexpr std::array<std::string_view, 4> kStateNames{"idle", "rolling", "scared", "unrolling"};

/// parity: `pickNextScuteDropTime`, five to ten minutes.
[[nodiscard]] std::int32_t nextScuteTime(random::RandomSource& random) { return random.nextInt(6000) + 6000; }

[[nodiscard]] bool inLiquid(const world::WorldState& world, const ServerEntity& mob) {
    const std::optional<std::int32_t> state = world.blockState(mth::floor(mob.x), mth::floor(mob.y), mth::floor(mob.z));
    return state.has_value() && world::fluids::of(*state) != world::Submersion::None;
}

} // namespace

namespace entity::armadillos {

ArmadilloState& stateOf(ServerEntity& armadillo) {
    if (armadillo.armadillo != nullptr) {
        return *armadillo.armadillo;
    }
    armadillo.armadillo = std::make_shared<ArmadilloState>();
    ArmadilloState& state = *armadillo.armadillo;
    if (armadillo.sourceNbt.has_value()) {
        const std::string_view name = armadillo.sourceNbt->getString("state");
        for (std::size_t index = 0; index < kStateNames.size(); ++index) {
            if (kStateNames[index] == name) {
                state.state = static_cast<State>(index);
            }
        }
        if (armadillo.sourceNbt->contains("scute_time")) {
            state.scuteTime = armadillo.sourceNbt->getInt("scute_time");
        }
    }
    return state;
}

bool isScared(const ServerEntity& armadillo) {
    return armadillo.armadillo != nullptr && armadillo.armadillo->state != State::Idle;
}

void write(const ServerEntity& armadillo, wire::nbt::NbtCompound& tag) {
    if (armadillo.armadillo == nullptr) {
        return;
    }
    tag.putString("state", std::string{kStateNames[static_cast<std::size_t>(armadillo.armadillo->state)]});
    if (armadillo.armadillo->scuteTime.has_value()) {
        tag.putInt("scute_time", *armadillo.armadillo->scuteTime);
    }
}

} // namespace entity::armadillos

void MobActionSystem::tickArmadillo(world::LoadedWorld& world, entity::ai::MobContext& context) {
    ServerEntity& armadillo = context.mob;
    ArmadilloState& state = armadillos::stateOf(armadillo);
    const std::int64_t now = context.gameTime;
    if (!state.scuteTime.has_value()) {
        state.scuteTime = nextScuteTime(context.random);
    }
    const bool liquid = inLiquid(context.world, armadillo);
    const bool baby = armadillo.age < 0;
    const auto switchTo = [&](State next) {
        state.state = next;
        state.inStateTicks = 0;
    };
    const auto emitAction = [&] {
        world::vibrations::emit(world.state(), "entity_action", armadillo.x, armadillo.y, armadillo.z, now,
                                {.entity = armadillo.uuid()});
    };
    // parity: `canStayRolledUp`.
    const auto canStayRolledUp = [&] {
        return state.panicTicks <= 0 && !liquid && !armadillo.leashHolderId.has_value() &&
               !armadillo.vehicleId.has_value() && armadillo.passengerIds.empty();
    };
    // parity: `rollUp`: it stops where it is, forgets love, and curls up.
    const auto rollUp = [&] {
        if (state.state != State::Idle) {
            return;
        }
        entity::ai::navigation::stop(armadillo);
        state.motion.walk.reset();
        armadillo.loveTicks = 0;
        emitAction();
        makeSound(world, armadillo, "entity.armadillo.roll", 1.0F);
        switchTo(State::Rolling);
    };
    const auto rollOut = [&] {
        if (state.state == State::Idle) {
            return;
        }
        emitAction();
        makeSound(world, armadillo, "entity.armadillo.unroll_finish", 1.0F);
        switchTo(State::Idle);
    };
    // parity: `actuallyHurt`: a blow from something living is danger, and a reason to curl up.
    if (armadillo.hurtBy.has_value()) {
        armadillo.hurtBy.reset();
        if (armadillo.health > 0.0) {
            state.danger.setFor(true, now, armadillos::kDangerTicks);
            if (canStayRolledUp()) {
                rollUp();
            }
        }
    }
    // parity: `ARMADILLO_SCARE_DETECTED`, every fifth turn: the undead, whoever hurt it last, and a
    // player running or riding, inside seven across and two up.
    if (armadillo.tickCount % 5 == 0) {
        if (!canStayRolledUp()) {
            state.danger.erase();
        } else {
            const double halfWidth = armadillo.width() / 2.0;
            const auto near = [&](double x, double y, double z, double width, double height) {
                return std::abs(x - armadillo.x) < halfWidth + 7.0 + (width / 2.0) && y + height > armadillo.y - 2.0 &&
                       y < armadillo.y + armadillo.height() + 2.0 &&
                       std::abs(z - armadillo.z) < halfWidth + 7.0 + (width / 2.0);
            };
            bool scared = false;
            for (const entity::ai::PlayerView& player : context.players) {
                if (!player.spectator &&
                    (player.sprinting || player.vehicleId.has_value() ||
                     armadillo.lastBlame.causingId == player.entityId) &&
                    near(player.x, player.y, player.z, player.width, player.height)) {
                    scared = true;
                }
            }
            for (const ServerEntity* other : context.entities) {
                if (scared) {
                    break;
                }
                if (other == &armadillo || other->removed ||
                    !near(other->x, other->y, other->z, other->width(), other->height())) {
                    continue;
                }
                const bool undead = options_.entityTags != nullptr &&
                                    world::realEntityTypeTags::membersOf(*options_.entityTags,
                                                                         wire::common::Identifier::vanilla("undead"))
                                        .contains(other->type());
                scared = undead || armadillo.lastBlame.causingId == other->entityId();
            }
            if (scared) {
                state.danger.setFor(true, now, armadillos::kDangerTicks);
            }
        }
    }
    if (!state.danger.has(now)) {
        state.danger.erase();
    }
    // parity: the core: `Swim(0.8)`, `ArmadilloPanic(2.0)` which unrolls it, and the rolling out.
    if (entity::ai::inWater(context.world, armadillo) && context.random.nextFloat() < 0.8F) {
        armadillo.velocityY += 0.04;
    }
    const bool wasPanicking = state.panicTicks > 0;
    const bool panicking = kit::panic(context, state.motion, state.panicTicks, 2.0F);
    if (panicking && !wasPanicking) {
        rollOut();
    }
    if (state.gazeCooldown > 0) {
        --state.gazeCooldown;
    }
    if (!state.danger.has(now) && state.state != State::Idle) {
        rollOut();
    }
    // parity: `updateActivity`: the ball while danger is fresh and it is not running, else idle.
    if (state.danger.has(now) && !panicking) {
        if (!state.ballUp && armadillo.onGround && !liquid) {
            state.ballUp = true;
            state.ballUpEnd = now + armadillos::kBallUpTicks;
            rollUp();
        }
    } else {
        kit::lookSometimes(context, state.look, state.lookTicker, 6.0, 30, 60, "player");
        state.breedTarget = kit::makeLove(context, state.motion, state.look, 1.0F, 1);
        const auto tempts = [this](const wire::common::Identifier& item) {
            return itemTagged("minecraft:armadillo_food", item);
        };
        const double closeEnough = baby ? 1.0 : 2.0;
        bool temptAsked = false;
        if (state.temptation.following) {
            temptAsked = true;
            static_cast<void>(
                kit::tempt(context, state.motion, state.look, state.temptation, 1.25F, tempts, closeEnough));
        } else {
            for (const std::size_t choice : kit::shuffled(context.random, {1, 1})) {
                if (choice == 0) {
                    temptAsked = true;
                    if (kit::tempt(context, state.motion, state.look, state.temptation, 1.25F, tempts, closeEnough)) {
                        break;
                    }
                } else if (kit::followAdult(context, state.motion, state.look, 5, 16, 1.25F)) {
                    break;
                }
            }
        }
        if (!temptAsked && state.temptation.cooldown > 0) {
            --state.temptation.cooldown;
        }
        kit::lookAround(context, state.look, state.gazeCooldown, 150, 250, 30.0F);
        // parity: the `RunOne` of a stroll, a walk to its look target and doing nothing.
        if (state.idleTicks > 0) {
            --state.idleTicks;
        } else if (!state.motion.walk.has_value()) {
            for (const std::size_t choice : kit::shuffled(context.random, {1, 1, 1})) {
                if (choice == 0) {
                    if (const std::optional<kit::Vec> spot = kit::strollSpot(context); spot.has_value()) {
                        kit::walkTo(state.motion, *spot, 1.0F, 0);
                    }
                    break;
                }
                if (choice == 1 && (state.look.entity.has_value() || state.look.at.has_value())) {
                    std::optional<kit::Vec> at = state.look.at;
                    if (state.look.entity.has_value()) {
                        at = kit::positionOf(context, *state.look.entity, false);
                    }
                    if (at.has_value()) {
                        kit::walkTo(state.motion, *at, 1.0F, 3);
                    }
                    break;
                }
                if (choice == 2) {
                    state.idleTicks = 30 + context.random.nextInt(31);
                    break;
                }
            }
        }
    }
    armadilloBallsUp(world, context, state);
    // parity: `customServerAiStep`'s scute, shed every five to ten minutes.
    if (armadillo.health > 0.0 && --*state.scuteTime <= 0) {
        if (armadilloDrops(world, armadillo, "gameplay/armadillo_shed")) {
            random::RandomSource voice = freshRandom();
            playSound(world, armadillo, "entity.armadillo.scute_drop", 1.0F,
                      ((voice.nextFloat() - voice.nextFloat()) * 0.2F) + 1.0F, SoundSource::Neutral);
            world::vibrations::emit(world.state(), "entity_place", armadillo.x, armadillo.y, armadillo.z, now,
                                    {.entity = armadillo.uuid()});
        }
        state.scuteTime = nextScuteTime(context.random);
    }
    // parity: `MoveToTargetSink` does not start on a scared one.
    if (state.state == State::Idle) {
        kit::moveToTarget(context, state.motion);
    }
    kit::lookAtTarget(context, state.look);
    ++state.inStateTicks;
}

void MobActionSystem::armadilloBallsUp(world::LoadedWorld& world, entity::ai::MobContext& context,
                                       ArmadilloState& state) {
    if (!state.ballUp) {
        return;
    }
    ServerEntity& armadillo = context.mob;
    const std::int64_t now = context.gameTime;
    // parity: `ArmadilloBallUp.canStillUse` and `stop`: it ends once the armadillo is idle again.
    if (now > state.ballUpEnd || state.state == State::Idle) {
        state.ballUp = false;
        const bool liquid = inLiquid(context.world, armadillo);
        if (state.panicTicks > 0 || liquid || armadillo.leashHolderId.has_value() || armadillo.vehicleId.has_value() ||
            !armadillo.passengerIds.empty()) {
            if (state.state != State::Idle) {
                makeSound(world, armadillo, "entity.armadillo.unroll_finish", 1.0F);
                state.state = State::Idle;
                state.inStateTicks = 0;
            }
        }
        return;
    }
    const auto sound = [&](std::string_view name) {
        if (!armadillo.sourceNbt.has_value() || !armadillo.sourceNbt->getBoolean("Silent")) {
            playSound(world, armadillo, name, 1.0F, 1.0F, SoundSource::Neutral);
        }
    };
    const auto switchTo = [&](State next) {
        state.state = next;
        state.inStateTicks = 0;
    };
    const auto nextPeek = [&] {
        return armadillos::animationDuration(State::Scared) + context.random.nextIntBetweenInclusive(100, 400);
    };
    if (state.nextPeek > 0) {
        --state.nextPeek;
    }
    // parity: `shouldSwitchToScaredState`, the roll run through.
    if (state.state == State::Rolling && state.inStateTicks > armadillos::animationDuration(State::Rolling)) {
        switchTo(State::Scared);
        if (armadillo.onGround) {
            sound("entity.armadillo.land");
        }
        return;
    }
    const std::int64_t dangerLeft = state.danger.has(now) ? state.danger.until - now : 0;
    const bool dangerAround = dangerLeft > armadillos::kDangerThreshold;
    if (dangerAround != state.dangerWasAround) {
        state.nextPeek = nextPeek();
    }
    state.dangerWasAround = dangerAround;
    if (state.state == State::Scared) {
        if (state.nextPeek == 0 && armadillo.onGround && dangerAround) {
            entityEvent(armadillo, armadillos::kPeekEvent);
            state.nextPeek = nextPeek();
        }
        if (dangerLeft < armadillos::animationDuration(State::Unrolling)) {
            sound("entity.armadillo.unroll_start");
            switchTo(State::Unrolling);
        }
    } else if (state.state == State::Unrolling && dangerLeft > armadillos::animationDuration(State::Unrolling)) {
        switchTo(State::Scared);
    }
}

bool MobActionSystem::armadilloDrops(world::LoadedWorld& world, const ServerEntity& armadillo,
                                     std::string_view tableId) {
    // parity: `dropFromGiftLootTable` and `dropFromEntityInteractLootTable`, spawned at its feet.
    const server::world::loot::LootTable* table =
        lootTables_->byId(wire::common::Identifier::vanilla(std::string{tableId}));
    if (table == nullptr) {
        return false;
    }
    random::RandomSource random = freshRandom();
    server::world::loot::LootContext context = deathLootContext(world, armadillo);
    random::RandomSource& roll =
        table->randomSequence.has_value() ? world.randomSequences().of(*table->randomSequence) : random;
    bool dropped = false;
    for (const server::world::loot::ItemStack& stack : table->generate(roll, context.luck, std::nullopt, context)) {
        dropped = true;
        world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                   .uuid = wire::common::Uuid::random(),
                                                   .item = stack.item,
                                                   .count = stack.count,
                                                   .x = armadillo.x,
                                                   .y = armadillo.y,
                                                   .z = armadillo.z,
                                                   .velocityX = (random.nextDouble() * 0.2) - 0.1,
                                                   .velocityY = 0.2,
                                                   .velocityZ = (random.nextDouble() * 0.2) - 0.1,
                                                   .pickupDelay = world::kSpawnedPickupDelay,
                                                   .components = stack.components});
    }
    return dropped;
}

bool MobActionSystem::brushArmadillo(world::LoadedWorld& world, ServerEntity& armadillo) {
    // parity: `Armadillo.brushOffScute`: never a baby; the brush table, its sound and the event.
    if (armadillo.age < 0) {
        return false;
    }
    static_cast<void>(armadilloDrops(world, armadillo, "brush/armadillo"));
    playSound(world, armadillo, "entity.armadillo.brush", 1.0F, 1.0F, SoundSource::Neutral);
    world::vibrations::emit(world.state(), "entity_interact", armadillo.x, armadillo.y, armadillo.z, world.gameTime(),
                            {.entity = armadillo.uuid()});
    return true;
}

} // namespace basalt::server::runtime
