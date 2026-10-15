#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Frogs.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <memory>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using wire::protocol::play::SoundSource;
namespace frogs = entity::frogs;
namespace kit = entity::ai::brainKit;
namespace mth = wire::common::mth;
namespace shapes = server::registry::blockShapes;
using frogs::Activity;
using frogs::FrogState;

constexpr std::string_view kLongJumpMemory = "minecraft:long_jump_cooldown_ticks";
constexpr std::string_view kPregnantMemory = "minecraft:is_pregnant";

/// parity: `Direction.Plane.HORIZONTAL`: north, east, south, west.
constexpr std::array<std::array<std::int32_t, 2>, 4> kHorizontal{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};

[[nodiscard]] world::Submersion fluidAt(const world::WorldState& world, const kit::Block& at) {
    const std::optional<std::int32_t> state = world.blockState(at[0], at[1], at[2]);
    return state.has_value() ? world::fluids::of(*state) : world::Submersion::None;
}

[[nodiscard]] bool waterAt(const world::WorldState& world, const kit::Block& at) {
    return fluidAt(world, at) == world::Submersion::Water;
}

[[nodiscard]] bool noCollision(const world::WorldState& world, const kit::Block& at) {
    const std::optional<std::int32_t> state = world.blockState(at[0], at[1], at[2]);
    return state.has_value() && shapes::isEmptyShape(*state);
}

[[nodiscard]] bool airAt(const world::WorldState& world, const kit::Block& at) {
    const std::optional<std::int32_t> state = world.blockState(at[0], at[1], at[2]);
    return state.has_value() && server::registry::blocks::isAir(*state);
}

[[nodiscard]] kit::Block blockOf(const ServerEntity& mob) {
    return {mth::floor(mob.x), mth::floor(mob.y), mth::floor(mob.z)};
}

[[nodiscard]] kit::Vec middleOf(const kit::Block& at) { return {at[0] + 0.5, at[1] + 0.5, at[2] + 0.5}; }

} // namespace

namespace entity::frogs {

FrogState& stateOf(ServerEntity& frog) {
    if (frog.frog != nullptr) {
        return *frog.frog;
    }
    frog.frog = std::make_shared<FrogState>();
    FrogState& state = *frog.frog;
    const wire::nbt::NbtCompound* brain = frog.sourceNbt.has_value() ? frog.sourceNbt->getCompound("Brain") : nullptr;
    const wire::nbt::NbtCompound* memories = brain == nullptr ? nullptr : brain->getCompound("memories");
    if (memories != nullptr) {
        state.seeded = true;
        if (const wire::nbt::NbtCompound* jump = memories->getCompound(kLongJumpMemory); jump != nullptr) {
            state.jump.cooldown = jump->getInt("value");
        }
        state.pregnant = memories->contains(kPregnantMemory);
    }
    return state;
}

TadpoleState& tadpoleOf(ServerEntity& tadpole) {
    if (tadpole.tadpole == nullptr) {
        tadpole.tadpole = std::make_shared<TadpoleState>();
    }
    return *tadpole.tadpole;
}

bool canEat(const ServerEntity& mob, bool inFoodTag) {
    const std::string_view path = mob.type().path();
    if ((path == "slime" || path == "magma_cube") && mob.cubeSize != 1) {
        return false;
    }
    return inFoodTag;
}

void write(const ServerEntity& frog, wire::nbt::NbtCompound& tag) {
    if (frog.frog == nullptr) {
        return;
    }
    wire::nbt::NbtCompound brain;
    if (const wire::nbt::NbtCompound* old = tag.getCompound("Brain"); old != nullptr) {
        brain = *old;
    }
    wire::nbt::NbtCompound memories;
    if (frog.frog->jump.cooldown.has_value()) {
        wire::nbt::NbtCompound slot;
        slot.putInt("value", *frog.frog->jump.cooldown);
        memories.set(kLongJumpMemory, wire::nbt::NbtTag::ofCompound(std::move(slot)));
    }
    if (frog.frog->pregnant) {
        // parity: `Unit.CODEC` writes an empty compound.
        wire::nbt::NbtCompound slot;
        slot.set("value", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
        memories.set(kPregnantMemory, wire::nbt::NbtTag::ofCompound(std::move(slot)));
    }
    brain.set("memories", wire::nbt::NbtTag::ofCompound(std::move(memories)));
    tag.set("Brain", wire::nbt::NbtTag::ofCompound(std::move(brain)));
}

} // namespace entity::frogs

void MobActionSystem::tickFrog(world::LoadedWorld& world, entity::ai::MobContext& context) {
    ServerEntity& frog = context.mob;
    FrogState& state = frogs::stateOf(frog);
    const std::int64_t now = context.gameTime;
    const bool inWater = entity::ai::inWater(context.world, frog);
    frog.smoothSwimming = inWater;
    if (!state.seeded) {
        // parity: `FrogAi.initMemories`, from `finalizeSpawn`.
        state.seeded = true;
        const auto& [low, high] = frogs::kTimeBetweenLongJumps;
        state.jump.cooldown = low + context.random.nextInt(high - low + 1);
    }
    if (now >= state.unreachableUntil) {
        state.unreachable.clear();
    }
    // parity: `StopAttackingIfTargetInvalid`, a dead or gone target let go.
    if (frog.targetId.has_value()) {
        const ServerEntity* target = context.entityById(*frog.targetId);
        if (target == nullptr || target->removed || target->dying) {
            frog.targetId.reset();
        }
    }
    // parity: `updateActivity`.
    if (frog.targetId.has_value()) {
        state.activity = Activity::Tongue;
    } else if (state.pregnant && !state.jump.midJump) {
        state.activity = Activity::LaySpawn;
    } else if (!state.temptation.following && !state.breedTarget.has_value() && !state.jump.cooldown.has_value() &&
               !inWater) {
        state.activity = Activity::LongJump;
    } else if (inWater && !state.jump.midJump) {
        state.activity = Activity::Swim;
    } else if (!inWater && !state.jump.midJump) {
        state.activity = Activity::Idle;
    }
    // parity: the core: `AnimalPanic(2.0)` and the countdowns.
    static_cast<void>(kit::panic(context, state.motion, state.panicTicks, 2.0F));
    entity::ai::longJump::countDown(state.jump);
    const auto tempts = [this](const wire::common::Identifier& item) {
        return itemTagged("minecraft:frog_food", item);
    };
    // parity: `StartAttacking` over `FrogAttackablesSensor`.
    const auto startAttacking = [&] {
        if (frog.targetId.has_value() || state.breedTarget.has_value()) {
            return;
        }
        for (const kit::Seen& seen : kit::nearby(context, 10.0)) {
            if (seen.mob == nullptr || seen.distanceSquared >= 100.0) {
                continue;
            }
            const bool food = options_.entityTags != nullptr &&
                              world::realEntityTypeTags::membersOf(*options_.entityTags,
                                                                   wire::common::Identifier::vanilla("frog_food"))
                                  .contains(seen.mob->type());
            if (frogs::canEat(*seen.mob, food) &&
                std::ranges::find(state.unreachable, seen.mob->uuid()) == state.unreachable.end()) {
                frog.targetId = seen.id;
                state.motion.cantReachSince.reset();
                return;
            }
        }
    };
    // parity: `SetWalkTargetFromLookTarget(1.0, 3)`.
    const auto walkToLook = [&] {
        if (state.motion.walk.has_value() || (!state.look.entity.has_value() && !state.look.at.has_value())) {
            return false;
        }
        std::optional<kit::Vec> at = state.look.at;
        if (state.look.entity.has_value()) {
            at = kit::positionOf(context, *state.look.entity, false);
        }
        if (at.has_value()) {
            kit::walkTo(state.motion, *at, 1.0F, 3);
        }
        return true;
    };
    // parity: the `RunOne` of a stroll.
    const auto runOne = [&](const std::vector<std::int32_t>& weights) {
        if (state.croaking) {
            return;
        }
        for (const std::size_t choice : kit::shuffled(context.random, weights)) {
            if (choice == 0 && !state.motion.walk.has_value()) {
                if (const std::optional<kit::Vec> spot = kit::strollSpot(context); spot.has_value()) {
                    kit::walkTo(state.motion, *spot, 1.0F, 0);
                }
                return;
            }
            if (choice == 1 && walkToLook()) {
                return;
            }
            if (choice == 2 && !state.motion.walk.has_value() && state.pose == frogs::Pose::Standing) {
                // parity: `Croak.start`, which leaves the old count alone in a liquid.
                state.croaking = true;
                if (fluidAt(context.world, blockOf(frog)) == world::Submersion::None) {
                    state.pose = frogs::Pose::Croaking;
                    state.croakCounter = 0;
                }
                return;
            }
            if (choice == 3 && frog.onGround) {
                return;
            }
        }
    };
    // parity: `TryFindLand(8, 1.5)`.
    const auto findLand = [&] {
        if (!waterAt(context.world, blockOf(frog)) || frog.targetId.has_value() || state.motion.walk.has_value()) {
            return;
        }
        // parity: a turn inside the cooldown still pushes it back.
        const bool due = now >= state.landSearch;
        state.landSearch = now + 60;
        if (!due) {
            return;
        }
        const kit::Block own = blockOf(frog);
        const std::optional<kit::Block> land =
            kit::nearestBlockWhere(context, 8, 8, [&](const kit::Block& at, std::int32_t stateId) {
                if ((at[0] == own[0] && at[2] == own[2]) || world::fluids::of(stateId) != world::Submersion::None ||
                    !shapes::isEmptyShape(stateId)) {
                    return false;
                }
                const std::optional<std::int32_t> below = context.world.blockState(at[0], at[1] - 1, at[2]);
                return below.has_value() && shapes::isFaceSturdy(*below, server::registry::BlockFace::Up);
            });
        if (land.has_value()) {
            state.look = kit::Look{.at = middleOf(*land)};
            kit::walkTo(state.motion, middleOf(*land), 1.5F, 1);
        }
    };
    switch (state.activity) {
    case Activity::Tongue:
        frogTongue(world, context, state);
        break;
    case Activity::LongJump:
        break;
    case Activity::LaySpawn:
        kit::lookSometimes(context, state.look, state.lookTicker, 6.0, 30, 60, "player");
        startAttacking();
        frogLaysSpawn(world, context, state, inWater);
        runOne({2, 1, 2, 1});
        break;
    case Activity::Swim:
        kit::lookSometimes(context, state.look, state.lookTicker, 6.0, 30, 60, "player");
        static_cast<void>(kit::tempt(context, state.motion, state.look, state.temptation, 1.25F, tempts));
        startAttacking();
        findLand();
        // parity: the gate tries a swim, a stroll and a walk to its look target, in that order.
        if (!state.motion.walk.has_value() && !state.croaking) {
            if (inWater) {
                if (const std::optional<kit::Vec> spot = kit::swimSpot(context); spot.has_value()) {
                    kit::walkTo(state.motion, *spot, 0.75F, 0);
                }
            }
            if (!state.motion.walk.has_value()) {
                if (const std::optional<kit::Vec> spot = kit::strollSpot(context); spot.has_value()) {
                    kit::walkTo(state.motion, *spot, 1.0F, 0);
                }
            }
            static_cast<void>(walkToLook());
        }
        break;
    case Activity::Idle:
        kit::lookSometimes(context, state.look, state.lookTicker, 6.0, 30, 60, "player");
        state.breedTarget = kit::makeLove(context, state.motion, state.look, 1.0F, 2);
        static_cast<void>(kit::tempt(context, state.motion, state.look, state.temptation, 1.25F, tempts));
        startAttacking();
        if (!state.motion.walk.has_value()) {
            runOne({1, 1, 3, 2});
        }
        break;
    }
    frogJumps(world, context, state);
    // parity: `Croak.tick`.
    if (state.croaking && ++state.croakCounter > frogs::kCroakTicks) {
        state.croaking = false;
        state.pose = frogs::Pose::Standing;
    }
    kit::moveToTarget(context, state.motion);
    kit::lookAtTarget(context, state.look);
}

void MobActionSystem::frogJumps(world::LoadedWorld& world, entity::ai::MobContext& context, FrogState& state) {
    // parity: `LongJumpToPreferredBlock(100..140, 2, 4)` in its activity.
    // running on until it lands whatever the activity.
    if (state.activity != Activity::LongJump && !state.jump.midJump) {
        return;
    }
    ServerEntity& frog = context.mob;
    const auto blockAt = [&context](const kit::Block& at) -> const wire::common::Identifier* {
        const std::optional<std::int32_t> stateId = context.world.blockState(at[0], at[1], at[2]);
        return stateId.has_value() ? server::registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    };
    const auto preferred = [&](const kit::Block& at) {
        const wire::common::Identifier* block = blockAt(at);
        return block != nullptr && blockTagged("minecraft:frog_prefer_jump_to", *block).value_or(false);
    };
    const auto trapdoor = [&](const kit::Block& at) {
        const wire::common::Identifier* block = blockAt(at);
        return block != nullptr && block->path().ends_with("_trapdoor");
    };
    const entity::ai::longJump::Spec spec{.cooldown = frogs::kTimeBetweenLongJumps,
                                          .height = 2,
                                          .width = 4,
                                          .acceptable =
                                              [&](const kit::Block& target) {
                                                  // parity: `FrogAi.isAcceptableLandingSpot`: dry all round, then a.
                                                  // preferred block, a trapdoor, or whatever the default takes.
                                                  const kit::Block below{target[0], target[1] - 1, target[2]};
                                                  if (fluidAt(context.world, target) != world::Submersion::None ||
                                                      fluidAt(context.world, below) != world::Submersion::None ||
                                                      fluidAt(context.world, {target[0], target[1] + 1, target[2]}) !=
                                                          world::Submersion::None) {
                                                      return false;
                                                  }
                                                  if (preferred(target) || preferred(below) || trapdoor(target) ||
                                                      (airAt(context.world, target) && trapdoor(below))) {
                                                      return true;
                                                  }
                                                  return entity::ai::longJump::defaultAcceptable(context, target);
                                              },
                                          .preferredChance = 0.5F,
                                          .preferred = preferred};
    switch (entity::ai::longJump::tick(context, state.jump, spec, state.look)) {
    case entity::ai::longJump::Event::Jumped:
        state.pose = frogs::Pose::LongJumping;
        playSound(world, frog, "entity.frog.long_jump", 1.0F, 1.0F, SoundSource::Neutral);
        break;
    case entity::ai::longJump::Event::Landed:
        if (state.pose == frogs::Pose::LongJumping) {
            state.pose = frogs::Pose::Standing;
        }
        playSound(world, frog, "entity.frog.step", 2.0F, 1.0F, SoundSource::Neutral);
        break;
    case entity::ai::longJump::Event::None:
        break;
    }
}

void MobActionSystem::frogTongue(world::LoadedWorld& world, entity::ai::MobContext& context, FrogState& state) {
    ServerEntity& frog = context.mob;
    ServerEntity* target = frog.targetId.has_value() ? context.entityById(*frog.targetId) : nullptr;
    const bool panicking = state.panicTicks > 0;
    const auto stop = [&] {
        frog.targetId.reset();
        state.tongueTarget.reset();
        state.pose = frogs::Pose::Standing;
        state.tongueRunning = false;
    };
    if (!state.tongueRunning) {
        if (target == nullptr || state.motion.walk.has_value() || panicking) {
            return;
        }
        // parity: `checkExtraStartConditions`.
        // remembered as out of reach.
        if (!entity::ai::navigation::canReach(context.world, frog, target->x, target->y, target->z)) {
            frog.targetId.reset();
            if (std::ranges::find(state.unreachable, target->uuid()) == state.unreachable.end()) {
                if (state.unreachable.size() == 5) {
                    state.unreachable.erase(state.unreachable.begin());
                }
                state.unreachable.push_back(target->uuid());
            }
            state.unreachableUntil = context.gameTime + 100;
            return;
        }
        if (state.pose == frogs::Pose::Croaking) {
            return;
        }
        // parity: `start`, which then gets its first tick in the same turn.
        state.tongueRunning = true;
        state.tongueEnd = context.gameTime + 100;
        state.look = kit::Look{.entity = target->entityId()};
        state.tongueTarget = target->entityId();
        kit::walkTo(state.motion, {target->x, target->y, target->z}, 2.0F, 0);
        state.pathCounter = 10;
        state.tongue = frogs::Tongue::MoveToTarget;
    }
    if (context.gameTime > state.tongueEnd || target == nullptr || state.tongue == frogs::Tongue::Done || panicking) {
        stop();
        return;
    }
    state.tongueTarget = target->entityId();
    switch (state.tongue) {
    case frogs::Tongue::MoveToTarget: {
        const double dx = frog.x - target->x;
        const double dy = frog.y - target->y;
        const double dz = frog.z - target->z;
        const double length = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
        if (static_cast<float>(length) < frogs::kEatingDistance) {
            playSound(world, frog, "entity.frog.tongue", 2.0F, 1.0F, SoundSource::Neutral);
            state.pose = frogs::Pose::UsingTongue;
            // parity: the catch pulls the prey towards the frog at three quarters of a block a tick.
            const double scale = length < static_cast<double>(1.0E-5F) ? 0.0 : static_cast<double>(0.75F) / length;
            target->velocityX = dx * scale;
            target->velocityY = dy * scale;
            target->velocityZ = dz * scale;
            state.eatTimer = 0;
            state.tongue = frogs::Tongue::CatchAnimation;
        } else if (state.pathCounter <= 0) {
            kit::walkTo(state.motion, {target->x, target->y, target->z}, 2.0F, 0);
            state.pathCounter = 10;
        } else {
            --state.pathCounter;
        }
        break;
    }
    case frogs::Tongue::CatchAnimation:
        if (state.eatTimer++ >= 6) {
            state.tongue = frogs::Tongue::EatAnimation;
            // parity: `eatEntity`: the gulp, the bite, and a kill taken off the level with no death.
            playSound(world, frog, "entity.frog.eat", 2.0F, 1.0F, SoundSource::Neutral);
            if (!target->removed && !target->dying) {
                hurtMob(frog, *target, static_cast<float>(frog.attributeWithModifiers("minecraft:attack_damage")));
                if (target->dying) {
                    target->deathTime = std::max(target->deathTime, entity::kDeathTicks - 1);
                }
            }
        }
        break;
    case frogs::Tongue::EatAnimation:
        if (state.eatTimer >= 10) {
            state.tongue = frogs::Tongue::Done;
        } else {
            ++state.eatTimer;
        }
        break;
    case frogs::Tongue::Done:
        break;
    }
}

void MobActionSystem::frogLaysSpawn(world::LoadedWorld& world, entity::ai::MobContext& context, FrogState& state,
                                    bool inWater) {
    ServerEntity& frog = context.mob;
    const std::int64_t now = context.gameTime;
    const kit::Block own = blockOf(frog);
    // parity: `TryFindLandNearLiquid(8, 1.0)`.
    if (!waterAt(context.world, own) && !frog.targetId.has_value() && !state.motion.walk.has_value()) {
        const bool due = now >= state.landNearLiquidSearch;
        state.landNearLiquidSearch = now + 40;
        if (due) {
            const std::optional<kit::Block> land =
                kit::nearestBlockWhere(context, 8, 8, [&](const kit::Block& at, std::int32_t stateId) {
                    if ((at[0] == own[0] && at[2] == own[2]) || !shapes::isEmptyShape(stateId) ||
                        noCollision(context.world, {at[0], at[1] - 1, at[2]})) {
                        return false;
                    }
                    return std::ranges::any_of(kHorizontal, [&](const std::array<std::int32_t, 2>& side) {
                        return airAt(context.world, {at[0] + side[0], at[1], at[2] + side[1]}) &&
                               waterAt(context.world, {at[0] + side[0], at[1] - 1, at[2] + side[1]});
                    });
                });
            if (land.has_value()) {
                state.look = kit::Look{.at = middleOf(*land)};
                kit::walkTo(state.motion, middleOf(*land), 1.0F, 0);
            }
        }
    }
    // parity: `TryLaySpawnOnFluidNearLand`: walking on the ground, spawn on the water beside.
    if (inWater || !frog.onGround || !state.motion.walk.has_value() || frog.targetId.has_value()) {
        return;
    }
    for (const auto& side : kHorizontal) {
        const kit::Block water{own[0] + side[0], own[1] - 1, own[2] + side[1]};
        const std::optional<std::int32_t> waterState = context.world.blockState(water[0], water[1], water[2]);
        const wire::common::Identifier* block =
            waterState.has_value() ? server::registry::blockStateDefinitions::blockOf(*waterState) : nullptr;
        const bool supports = waterAt(context.world, water) ||
                              (block != nullptr && blockTagged("minecraft:supports_frogspawn", *block).value_or(false));
        const kit::Block spawnAt{water[0], water[1] + 1, water[2]};
        if (!noCollision(context.world, water) || !supports || !airAt(context.world, spawnAt)) {
            continue;
        }
        const std::optional<std::int32_t> spawn =
            server::registry::blocks::defaultStateIdOf(wire::common::Identifier::vanilla("frogspawn"));
        if (spawn.has_value() &&
            villagersIn(world).setBlock(frog, world::poi::BlockPos{spawnAt[0], spawnAt[1], spawnAt[2]}, *spawn)) {
            world::vibrations::emit(world.state(), "block_place", spawnAt[0] + 0.5, spawnAt[1] + 0.5, spawnAt[2] + 0.5,
                                    now, {.entity = frog.uuid()});
            playSound(world, frog, "entity.frog.lay_spawn", 1.0F, 1.0F, SoundSource::Blocks);
            state.pregnant = false;
        }
        return;
    }
}

void MobActionSystem::tickTadpole(world::LoadedWorld& /*world*/, entity::ai::MobContext& context) {
    ServerEntity& tadpole = context.mob;
    frogs::TadpoleState& state = frogs::tadpoleOf(tadpole);
    const bool inWater = entity::ai::inWater(context.world, tadpole);
    tadpole.smoothSwimming = inWater;
    // parity: `TadpoleAi`.
    // swim and a walk to what it looks at.
    if (!kit::panic(context, state.motion, state.panicTicks, 2.0F)) {
        kit::lookSometimes(context, state.look, state.lookTicker, 6.0, 30, 60, "player");
        static_cast<void>(kit::tempt(
            context, state.motion, state.look, state.temptation, 1.25F,
            [this](const wire::common::Identifier& item) { return itemTagged("minecraft:frog_food", item); }));
        if (!state.motion.walk.has_value()) {
            if (inWater) {
                if (const std::optional<kit::Vec> spot = kit::swimSpot(context); spot.has_value()) {
                    kit::walkTo(state.motion, *spot, 0.5F, 0);
                }
            }
            if (!state.motion.walk.has_value() && state.look.entity.has_value()) {
                if (const std::optional<kit::Vec> at = kit::positionOf(context, *state.look.entity, false);
                    at.has_value()) {
                    kit::walkTo(state.motion, *at, 0.5F, 3);
                }
            }
        }
    }
    kit::moveToTarget(context, state.motion);
    kit::lookAtTarget(context, state.look);
}

} // namespace basalt::server::runtime
