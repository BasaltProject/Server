#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/Goats.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::protocol::play::Gamemode;
namespace goats = entity::goats;
namespace kit = entity::ai::brainKit;
namespace mth = wire::common::mth;
using goats::Activity;
using goats::Block;
using goats::GoatState;
using goats::Vec;

[[nodiscard]] std::int32_t sample(random::RandomSource& random, const std::array<std::int32_t, 2>& range) {
    return range[0] + random.nextInt(range[1] - range[0] + 1);
}

[[nodiscard]] Block blockOf(const ServerEntity& mob) {
    return {mth::floor(mob.x), mth::floor(mob.y), mth::floor(mob.z)};
}

[[nodiscard]] bool isBaby(const ServerEntity& mob) { return mob.age < 0; }

/// parity: `GoatAi.RAM_TARGET_CONDITIONS`.
struct RamCandidate {
    std::int32_t id = 0;
    Vec at{};
    double width = 0.6;
    double height = 1.8;
    bool player = false;
};

} // namespace

namespace entity::goats {
namespace {

constexpr std::string_view kLongJumpMemory = "minecraft:long_jump_cooldown_ticks";
constexpr std::string_view kRamMemory = "minecraft:ram_cooldown_ticks";

} // namespace

GoatState& stateOf(ServerEntity& goat) {
    if (goat.goat != nullptr) {
        return *goat.goat;
    }
    goat.goat = std::make_shared<GoatState>();
    GoatState& state = *goat.goat;
    const wire::nbt::NbtCompound* brain = goat.sourceNbt.has_value() ? goat.sourceNbt->getCompound("Brain") : nullptr;
    const wire::nbt::NbtCompound* memories = brain == nullptr ? nullptr : brain->getCompound("memories");
    if (memories != nullptr) {
        // parity: the two cooldowns the brain saves; a goat without them was never spawned by the jar.
        state.seeded = true;
        if (const wire::nbt::NbtCompound* jump = memories->getCompound(kLongJumpMemory); jump != nullptr) {
            state.jump.cooldown = jump->getInt("value");
        }
        if (const wire::nbt::NbtCompound* ram = memories->getCompound(kRamMemory); ram != nullptr) {
            state.ramCooldown = ram->getInt("value");
        }
    }
    return state;
}

void write(const ServerEntity& goat, wire::nbt::NbtCompound& tag) {
    if (goat.goat == nullptr) {
        return;
    }
    wire::nbt::NbtCompound brain;
    if (const wire::nbt::NbtCompound* old = tag.getCompound("Brain"); old != nullptr) {
        brain = *old;
    }
    wire::nbt::NbtCompound memories;
    const auto put = [&memories](std::string_view name, const std::optional<std::int32_t>& value) {
        if (value.has_value()) {
            wire::nbt::NbtCompound slot;
            slot.putInt("value", *value);
            memories.set(name, wire::nbt::NbtTag::ofCompound(std::move(slot)));
        }
    };
    put(kLongJumpMemory, goat.goat->jump.cooldown);
    put(kRamMemory, goat.goat->ramCooldown);
    brain.set("memories", wire::nbt::NbtTag::ofCompound(std::move(memories)));
    tag.set("Brain", wire::nbt::NbtTag::ofCompound(std::move(brain)));
}

bool isScreaming(const ServerEntity& goat) {
    return goat.sourceNbt.has_value() && goat.sourceNbt->getBoolean("IsScreamingGoat");
}

} // namespace entity::goats

void MobActionSystem::tickGoat(world::LoadedWorld& world, entity::ai::MobContext& context) {
    ServerEntity& goat = context.mob;
    GoatState& state = goats::stateOf(goat);
    const bool screaming = goats::isScreaming(goat);
    if (!state.seeded) {
        // parity: `GoatAi.initMemories`, from `finalizeSpawn` and breeding.
        state.seeded = true;
        state.jump.cooldown = sample(context.random, goats::kTimeBetweenLongJumps);
        state.ramCooldown = sample(context.random, goats::kTimeBetweenRams);
    }
    // parity: the core: `Swim(0.8)`, `AnimalPanic(2.0)` and the three countdowns.
    if (entity::ai::inWater(context.world, goat) && context.random.nextFloat() < 0.8F) {
        goat.velocityY += 0.04;
    }
    const bool panicking = kit::panic(context, state.motion, state.panicTicks, 2.0F);
    entity::ai::longJump::countDown(state.jump);
    if (state.ramCooldown.has_value() && --*state.ramCooldown <= 0) {
        state.ramCooldown.reset();
    }
    const bool tempted = state.temptation.following;
    // parity: `updateActivity`: ram, long jump or idle, whichever's conditions hold first.
    if (!tempted && !state.breedTarget.has_value() && !state.ramCooldown.has_value()) {
        state.activity = Activity::Ram;
    } else if (!tempted && !state.breedTarget.has_value() && !state.motion.walk.has_value() &&
               !state.jump.cooldown.has_value()) {
        state.activity = Activity::LongJump;
    } else if (state.ramCooldown.has_value() && state.jump.cooldown.has_value()) {
        state.activity = Activity::Idle;
    }
    if (state.jump.midJump || state.activity == Activity::LongJump) {
        // parity: `LongJumpToRandomPos(600..1200, 5, 5)` with the goat's own sounds.
        static const entity::ai::longJump::Spec kSpec{.cooldown = goats::kTimeBetweenLongJumps,
                                                      .height = goats::kLongJumpHeight,
                                                      .width = goats::kLongJumpWidth,
                                                      .velocityMultiplier = goats::kMaxJumpVelocityMultiplier,
                                                      .jumpingScale = 0.7F};
        switch (entity::ai::longJump::tick(context, state.jump, kSpec, state.look)) {
        case entity::ai::longJump::Event::Jumped:
            playSound(world, goat, screaming ? "entity.goat.screaming.long_jump" : "entity.goat.long_jump", 1.0F, 1.0F);
            break;
        case entity::ai::longJump::Event::Landed:
            playSound(world, goat, "entity.goat.step", 2.0F, 1.0F);
            break;
        case entity::ai::longJump::Event::None:
            break;
        }
    } else if (state.activity == Activity::Ram) {
        goatRam(world, context, state, screaming);
    } else if (!panicking) {
        kit::lookSometimes(context, state.look, state.lookTicker, 6.0, 30, 60, "player");
        state.breedTarget = kit::makeLove(context, state.motion, state.look, 1.0F, 2);
        if (!state.breedTarget.has_value() &&
            !kit::tempt(context, state.motion, state.look, state.temptation, 1.25F,
                        [](const wire::common::Identifier& item) {
                            return entity::breeding::isFood(wire::common::Identifier::vanilla("goat"), item);
                        }) &&
            !kit::followAdult(context, state.motion, state.look, 5, 16, 1.25F)) {
            kit::idleMovement(context, state.motion, state.look, state.idleTicks, 1.0F);
        }
    }
    kit::moveToTarget(context, state.motion);
    kit::lookAtTarget(context, state.look);
}

void MobActionSystem::goatRam(world::LoadedWorld& world, entity::ai::MobContext& context, GoatState& state,
                              bool screaming) {
    ServerEntity& goat = context.mob;
    const std::int64_t now = context.gameTime;
    const std::array<std::int32_t, 2>& between = screaming ? goats::kTimeBetweenRamsScreamer : goats::kTimeBetweenRams;
    const bool griefing = world.gameRules().get(world::gameRules::kMobGriefing);
    const auto candidates = [&]() {
        std::vector<RamCandidate> found;
        for (const kit::Seen& seen : kit::nearby(context, goat.attributeWithModifiers("minecraft:follow_range"))) {
            if (seen.player != nullptr) {
                if (seen.player->targetable) {
                    found.push_back(RamCandidate{.id = seen.id,
                                                 .at = {seen.player->x, seen.player->y, seen.player->z},
                                                 .width = seen.player->width,
                                                 .height = seen.player->height,
                                                 .player = true});
                }
                continue;
            }
            if (seen.type == "goat" || (!griefing && seen.type == "armor_stand")) {
                continue;
            }
            found.push_back(RamCandidate{.id = seen.id,
                                         .at = {seen.mob->x, seen.mob->y, seen.mob->z},
                                         .width = seen.mob->width(),
                                         .height = seen.mob->height()});
        }
        return found;
    };
    const auto finish = [&]() {
        entityEvent(goat, 59);
        state.ramCooldown = sample(context.random, between);
        state.ramTarget.reset();
        state.ramming = false;
    };
    // parity: `RamTarget`, which runs once `RAM_TARGET` is set.
    if (state.ramTarget.has_value()) {
        if (!state.ramming) {
            state.ramming = true;
            state.ramRun = 0;
            const Block own = blockOf(goat);
            const double dx = own[0] - (*state.ramTarget)[0];
            const double dz = own[2] - (*state.ramTarget)[2];
            const double length = std::sqrt((dx * dx) + (dz * dz));
            state.ramDirection = length < 1.0E-4 ? Vec{} : Vec{dx / length, 0.0, dz / length};
            kit::walkTo(state.motion, *state.ramTarget, 3.0F, 0);
        }
        if (++state.ramRun > 200) {
            finish();
            return;
        }
        const entity::Aabb box = entity::Aabb::of(goat.x, goat.y, goat.z, goat.width(), goat.height());
        for (const RamCandidate& candidate : candidates()) {
            const entity::Aabb theirs =
                entity::Aabb::of(candidate.at[0], candidate.at[1], candidate.at[2], candidate.width, candidate.height);
            if (!box.intersects(theirs)) {
                continue;
            }
            goatRams(world, goat, candidate.id, candidate.player, state.ramDirection);
            finish();
            playSound(world, goat, screaming ? "entity.goat.screaming.ram_impact" : "entity.goat.ram_impact", 1.0F,
                      1.0F);
            return;
        }
        // parity: `hasRammedHornBreakingBlock`.
        const double hx = goat.velocityX;
        const double hz = goat.velocityZ;
        const double hLength = std::sqrt((hx * hx) + (hz * hz));
        if (hLength > 1.0E-4) {
            const std::int32_t fx = mth::floor(goat.x + (hx / hLength));
            const std::int32_t fy = mth::floor(goat.y);
            const std::int32_t fz = mth::floor(goat.z + (hz / hLength));
            bool snaps = false;
            for (const std::int32_t y : {fy, fy + 1}) {
                const std::optional<std::int32_t> stateId = context.world.blockState(fx, y, fz);
                const wire::common::Identifier* block =
                    stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
                snaps = snaps || (block != nullptr && blockTagged("minecraft:snaps_goat_horn", *block).value_or(false));
            }
            if (snaps) {
                playSound(world, goat, screaming ? "entity.goat.screaming.ram_impact" : "entity.goat.ram_impact", 1.0F,
                          1.0F);
                if (goatDropsHorn(world, goat, context.random)) {
                    playSound(world, goat, "entity.goat.horn_break", 1.0F, 1.0F);
                }
                finish();
                return;
            }
        }
        const bool reached =
            !state.motion.walk.has_value() || std::pow(state.motion.walk->at[0] - (*state.ramTarget)[0], 2) +
                                                      std::pow(state.motion.walk->at[1] - (*state.ramTarget)[1], 2) +
                                                      std::pow(state.motion.walk->at[2] - (*state.ramTarget)[2], 2) <
                                                  0.0625;
        if (reached && !state.motion.walk.has_value()) {
            finish();
        }
        return;
    }
    // parity: `PrepareRamNearestTarget`.
    // ticks of lowered head, then the charge.
    const auto chooseStart = [&](std::int32_t targetId, const Vec& at) {
        state.reachedRamStart.reset();
        state.ramStart.reset();
        const Block target{mth::floor(at[0]), mth::floor(at[1]), mth::floor(at[2])};
        const auto walkable = [&](const Block& pos) {
            const std::optional<std::int32_t> stateId = context.world.blockState(pos[0], pos[1], pos[2]);
            const std::optional<std::int32_t> below = context.world.blockState(pos[0], pos[1] - 1, pos[2]);
            return stateId.has_value() && below.has_value() && registry::blockShapes::isEmptyShape(*stateId) &&
                   !registry::blockShapes::isEmptyShape(*below);
        };
        if (!walkable(target)) {
            return;
        }
        std::vector<Block> starts;
        constexpr std::array<std::array<std::int32_t, 2>, 4> kSides{{{0, -1}, {0, 1}, {-1, 0}, {1, 0}}};
        for (const auto& side : kSides) {
            Block furthest = target;
            for (std::int32_t distance = 0; distance < goats::kRamMaxDistance; ++distance) {
                const Block next{furthest[0] + side[0], furthest[1], furthest[2] + side[1]};
                if (!walkable(next)) {
                    break;
                }
                furthest = next;
            }
            if (std::abs(furthest[0] - target[0]) + std::abs(furthest[2] - target[2]) >= goats::kRamMinDistance) {
                starts.push_back(furthest);
            }
        }
        const Block own = blockOf(goat);
        std::ranges::stable_sort(starts, {}, [&own](const Block& pos) {
            return std::pow(pos[0] - own[0], 2) + std::pow(pos[1] - own[1], 2) + std::pow(pos[2] - own[2], 2);
        });
        for (const Block& start : starts) {
            if (entity::ai::navigation::canReach(context.world, goat, start[0] + 0.5, start[1], start[2] + 0.5)) {
                state.ramStart = start;
                state.ramTargetBlock = target;
                state.ramEntity = targetId;
                return;
            }
        }
    };
    if (!state.preparing) {
        state.preparing = true;
        state.prepareRun = 0;
        state.ramEntity.reset();
        state.ramStart.reset();
        const std::vector<RamCandidate> found = candidates();
        if (!found.empty()) {
            chooseStart(found.front().id, found.front().at);
        }
    }
    const std::optional<Vec> target =
        state.ramEntity.has_value() ? kit::positionOf(context, *state.ramEntity, false) : std::nullopt;
    if (!state.ramStart.has_value() || !target.has_value() || ++state.prepareRun > 160) {
        // parity: `stop` without a target: the head back up and the shorter cooldown.
        state.preparing = false;
        entityEvent(goat, 59);
        state.ramCooldown = between[0];
        return;
    }
    kit::walkTo(state.motion,
                {(*state.ramStart)[0] + 0.5, static_cast<double>((*state.ramStart)[1]), (*state.ramStart)[2] + 0.5},
                1.25F, 0);
    state.look = kit::Look{.entity = state.ramEntity};
    const Block now_target{mth::floor((*target)[0]), mth::floor((*target)[1]), mth::floor((*target)[2])};
    if (now_target != *state.ramTargetBlock) {
        entityEvent(goat, 59);
        entity::ai::navigation::stop(goat);
        chooseStart(*state.ramEntity, *target);
        return;
    }
    if (blockOf(goat) == *state.ramStart) {
        entityEvent(goat, 58);
        if (!state.reachedRamStart.has_value()) {
            state.reachedRamStart = now;
        }
        if (now - *state.reachedRamStart >= goats::kRamPrepareTime) {
            // parity: `getEdgeOfBlock`, the near edge of the target's block.
            const Block start = *state.ramStart;
            const Block end = *state.ramTargetBlock;
            const double xOffset = 0.5 * static_cast<double>((end[0] > start[0]) - (end[0] < start[0]));
            const double zOffset = 0.5 * static_cast<double>((end[2] > start[2]) - (end[2] < start[2]));
            state.ramTarget = Vec{end[0] + 0.5 + xOffset, static_cast<double>(end[1]), end[2] + 0.5 + zOffset};
            makeSound(world, goat, screaming ? "entity.goat.screaming.prepare_ram" : "entity.goat.prepare_ram", 1.0F);
            state.preparing = false;
            state.motion.walk.reset();
        }
    }
}

void MobActionSystem::goatRams(world::LoadedWorld& world, ServerEntity& goat, std::int32_t targetId, bool player,
                               const std::array<double, 3>& direction) {
    // parity: `RamTarget.tick`'s blow.
    // halved through a shield.
    const float damage = static_cast<float>(goat.attributeWithModifiers("minecraft:attack_damage"));
    const double speedFactor =
        std::clamp(goat.attributeWithModifiers("minecraft:movement_speed") * 3.0 * 1.65, 0.2, 3.0);
    const double power = speedFactor * (isBaby(goat) ? 1.0 : 2.5);
    const double length = std::sqrt((direction[0] * direction[0]) + (direction[2] * direction[2]));
    const double kx = length < 1.0E-5 ? 0.0 : direction[0] / length;
    const double kz = length < 1.0E-5 ? 0.0 : direction[2] / length;
    if (player) {
        for (PlayerSession* session : watchersOf(world)) {
            if (session->entityId() != targetId) {
                continue;
            }
            hurtPlayer(goat, targetId, damage);
            if (session->connection().pushBack) {
                session->connection().pushBack(-kx * power, std::min(0.4, power), -kz * power, goat.yaw);
            }
        }
        return;
    }
    ServerEntity* target = world.state().entity(targetId);
    if (target == nullptr) {
        return;
    }
    hurtMob(goat, *target, damage);
    const double resisted = power * (1.0 - target->attributeWithModifiers("minecraft:knockback_resistance"));
    if (resisted <= 0.0) {
        return;
    }
    target->velocityX = (target->velocityX / 2.0) - (kx * resisted);
    target->velocityY = target->onGround ? std::min(0.4, (target->velocityY / 2.0) + resisted) : target->velocityY;
    target->velocityZ = (target->velocityZ / 2.0) - (kz * resisted);
}

bool MobActionSystem::goatDropsHorn(world::LoadedWorld& world, ServerEntity& goat, random::RandomSource& random) {
    // parity: `Goat.dropHorn`.
    if (isBaby(goat) || !goat.sourceNbt.has_value()) {
        return false;
    }
    wire::nbt::NbtCompound& tag = *goat.sourceNbt;
    const bool left = tag.getBoolean("HasLeftHorn", true);
    const bool right = tag.getBoolean("HasRightHorn", true);
    if (!left && !right) {
        return false;
    }
    const bool dropLeft = !right || (left && random.nextBoolean());
    tag.putBoolean(dropLeft ? "HasLeftHorn" : "HasRightHorn", false);
    // parity: `createHorn`, an instrument of the goat's kind picked from its uuid's hash.
    constexpr std::array<std::string_view, 4> kRegular{"ponder_goat_horn", "sing_goat_horn", "seek_goat_horn",
                                                       "feel_goat_horn"};
    constexpr std::array<std::string_view, 4> kScreaming{"admire_goat_horn", "call_goat_horn", "yearn_goat_horn",
                                                         "dream_goat_horn"};
    const std::uint64_t hilo = goat.uuid().high() ^ goat.uuid().low();
    const auto hash =
        static_cast<std::int32_t>(static_cast<std::uint32_t>(hilo >> 32) ^ static_cast<std::uint32_t>(hilo));
    random::RandomSource pick = random::RandomSource::legacy(hash);
    const auto& instruments = goats::isScreaming(goat) ? kScreaming : kRegular;
    wire::nbt::NbtCompound components;
    components.putString("minecraft:instrument",
                         "minecraft:" + std::string{instruments[static_cast<std::size_t>(pick.nextInt(4))]});
    world::DroppedItem horn{.entityId = players_->allocateEntityId(),
                            .uuid = wire::common::Uuid::random(),
                            .item = wire::common::Identifier::vanilla("goat_horn"),
                            .count = 1,
                            .x = goat.x,
                            .y = goat.y,
                            .z = goat.z,
                            .components = std::move(components)};
    horn.velocityX = static_cast<double>((random.nextFloat() * 0.4F) - 0.2F);
    horn.velocityY = static_cast<double>((random.nextFloat() * 0.4F) + 0.3F);
    horn.velocityZ = static_cast<double>((random.nextFloat() * 0.4F) - 0.2F);
    horn.pickupDelay = world::kSpawnedPickupDelay;
    world.state().spawnItem(std::move(horn));
    return true;
}

bool MobActionSystem::milkGoat(world::LoadedWorld& world, PlayerSession& player, ServerEntity& goat,
                               std::int32_t slot) {
    const std::optional<storage::StoredItemStack> held = player.inventory.at(slot);
    if (!held.has_value() || held->id != "minecraft:bucket" || isBaby(goat)) {
        return false;
    }
    playSound(world, goat, goats::isScreaming(goat) ? "entity.goat.screaming.milk" : "entity.goat.milk", 1.0F, 1.0F);
    // parity: `ItemUtils.createFilledResult`.
    const storage::StoredItemStack milk{.id = "minecraft:milk_bucket", .count = 1};
    if (player.gamemode == Gamemode::Creative) {
        bool has = false;
        for (std::int32_t index = 0; index < player::PlayerInventory::kMainSlots; ++index) {
            has = has || (player.inventory.at(index).has_value() && player.inventory.at(index)->id == milk.id);
        }
        if (!has) {
            static_cast<void>(player.inventory.add(milk));
        }
    } else if (held->count <= 1) {
        player.inventory.set(slot, milk);
    } else {
        storage::StoredItemStack left = *held;
        left.count -= 1;
        player.inventory.set(slot, left);
        if (player.inventory.add(milk) > 0) {
            world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                       .uuid = wire::common::Uuid::random(),
                                                       .item = wire::common::Identifier::vanilla("milk_bucket"),
                                                       .count = 1,
                                                       .x = player.x,
                                                       .y = player.y + 1.32,
                                                       .z = player.z,
                                                       .pickupDelay = 40});
        }
    }
    return true;
}

} // namespace basalt::server::runtime
