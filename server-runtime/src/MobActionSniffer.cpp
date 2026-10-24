#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Sniffers.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/RandomPos.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using wire::protocol::play::SoundSource;
namespace sniffers = entity::sniffers;
namespace kit = entity::ai::brainKit;
namespace mth = wire::common::mth;
using sniffers::Activity;
using sniffers::Pose;
using sniffers::Running;
using sniffers::SnifferState;

constexpr std::string_view kExploredMemory = "minecraft:sniffer_explored_positions";
constexpr std::string_view kCooldownMemory = "minecraft:sniff_cooldown";
constexpr float kDegreesToRadians = static_cast<float>(std::numbers::pi) / 180.0F;
/// parity: `storeExploredPosition`: twenty kept, then the new one in front.
constexpr std::size_t kExploredKept = 20;

/// parity: `Sniffer.getHeadBlock`.
[[nodiscard]] kit::Block headBlock(const ServerEntity& sniffer) {
    const float yCos =
        mth::cos(static_cast<double>((-sniffer.yaw * kDegreesToRadians) - static_cast<float>(std::numbers::pi)));
    const float ySin =
        mth::sin(static_cast<double>((-sniffer.yaw * kDegreesToRadians) - static_cast<float>(std::numbers::pi)));
    const float xCos = -mth::cos(static_cast<double>(-sniffer.pitch * kDegreesToRadians));
    return {mth::floor(sniffer.x + (static_cast<double>(ySin * xCos) * 2.25)),
            mth::floor(sniffer.y + static_cast<double>(0.2F)),
            mth::floor(sniffer.z + (static_cast<double>(yCos * xCos) * 2.25))};
}

} // namespace

namespace entity::sniffers {

SnifferState& stateOf(ServerEntity& sniffer) {
    if (sniffer.sniffer != nullptr) {
        return *sniffer.sniffer;
    }
    sniffer.sniffer = std::make_shared<SnifferState>();
    SnifferState& state = *sniffer.sniffer;
    const wire::nbt::NbtCompound* brain =
        sniffer.sourceNbt.has_value() ? sniffer.sourceNbt->getCompound("Brain") : nullptr;
    const wire::nbt::NbtCompound* memories = brain == nullptr ? nullptr : brain->getCompound("memories");
    if (memories == nullptr) {
        return state;
    }
    if (const wire::nbt::NbtCompound* explored = memories->getCompound(kExploredMemory); explored != nullptr) {
        if (const wire::nbt::NbtList* list = explored->getList("value"); list != nullptr) {
            for (const wire::nbt::NbtTag& tag : list->elements()) {
                const wire::nbt::NbtCompound* entry = tag.get<wire::nbt::NbtCompound>();
                const std::span<const std::int32_t> pos =
                    entry == nullptr ? std::span<const std::int32_t>{} : entry->getIntArray("pos");
                if (pos.size() == 3) {
                    state.explored.emplace_back(entry->getString("dimension"),
                                                ai::brainKit::Block{pos[0], pos[1], pos[2]});
                }
            }
        }
    }
    if (const wire::nbt::NbtCompound* cooldown = memories->getCompound(kCooldownMemory); cooldown != nullptr) {
        state.loadedCooldown = cooldown->contains("ttl") ? cooldown->getLong("ttl") : kSniffCooldown;
    }
    return state;
}

bool canMateNow(const ServerEntity& sniffer) {
    if (sniffer.sniffer == nullptr) {
        return true;
    }
    const Pose pose = sniffer.sniffer->pose;
    return pose == Pose::Idling || pose == Pose::Scenting || pose == Pose::FeelingHappy;
}

void write(const ServerEntity& sniffer, wire::nbt::NbtCompound& tag) {
    if (sniffer.sniffer == nullptr) {
        return;
    }
    const SnifferState& state = *sniffer.sniffer;
    wire::nbt::NbtCompound brain;
    if (const wire::nbt::NbtCompound* old = tag.getCompound("Brain"); old != nullptr) {
        brain = *old;
    }
    wire::nbt::NbtCompound memories;
    if (!state.explored.empty()) {
        wire::nbt::NbtList list{wire::nbt::NbtType::Compound};
        for (const auto& [dimension, pos] : state.explored) {
            wire::nbt::NbtCompound entry;
            entry.putString("dimension", dimension);
            entry.putIntArray("pos", {pos[0], pos[1], pos[2]});
            static_cast<void>(list.push(wire::nbt::NbtTag::ofCompound(std::move(entry))));
        }
        wire::nbt::NbtCompound slot;
        slot.set("value", wire::nbt::NbtTag::ofList(std::move(list)));
        memories.set(kExploredMemory, wire::nbt::NbtTag::ofCompound(std::move(slot)));
    }
    const std::optional<std::int64_t> ttl = state.loadedCooldown.has_value() ? state.loadedCooldown
                                            : state.sniffCooldown.has(state.lastTick)
                                                ? std::optional{state.sniffCooldown.until - state.lastTick}
                                                : std::nullopt;
    if (ttl.has_value()) {
        wire::nbt::NbtCompound slot;
        slot.set("value", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
        slot.putLong("ttl", std::max<std::int64_t>(*ttl, 0));
        memories.set(kCooldownMemory, wire::nbt::NbtTag::ofCompound(std::move(slot)));
    }
    brain.set("memories", wire::nbt::NbtTag::ofCompound(std::move(memories)));
    tag.set("Brain", wire::nbt::NbtTag::ofCompound(std::move(brain)));
}

} // namespace entity::sniffers

void MobActionSystem::tickSniffer(world::LoadedWorld& world, entity::ai::MobContext& context) {
    ServerEntity& sniffer = context.mob;
    SnifferState& state = sniffers::stateOf(sniffer);
    const std::int64_t now = context.gameTime;
    state.lastTick = now;
    if (state.loadedCooldown.has_value()) {
        state.sniffCooldown.setFor(true, now, *std::exchange(state.loadedCooldown, std::nullopt));
    }
    const bool inWater = entity::ai::inWater(context.world, sniffer);
    const bool baby = sniffer.age < 0;
    // parity: the core: `Swim(0.8)`, the panic that forgets any sniffing, and the countdowns.
    if (inWater && context.random.nextFloat() < 0.8F) {
        sniffer.velocityY += 0.04;
    }
    const bool wasPanicking = state.panicTicks > 0;
    const bool panicking = kit::panic(context, state.motion, state.panicTicks, 2.0F);
    const auto tempted = [&state] { return state.temptation.following; };
    // parity: `Sniffer.canSniff`.
    const auto canSniff = [&] {
        return !tempted() && state.panicTicks <= 0 && !inWater && sniffer.loveTicks <= 0 && sniffer.onGround &&
               !sniffer.vehicleId.has_value() && !sniffer.leashHolderId.has_value();
    };
    const std::string dimension = config::dimensionRegistryKey(world.declaration().dimension);
    // parity: `Sniffer.canDig(BlockPos)`: diggable, not dug here before, and a path there.
    const auto canDigAt = [&](const kit::Block& at) {
        const std::optional<std::int32_t> stateId = context.world.blockState(at[0], at[1], at[2]);
        const wire::common::Identifier* block =
            stateId.has_value() ? server::registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
        if (block == nullptr || !blockTagged("minecraft:sniffer_diggable_block", *block).value_or(false)) {
            return false;
        }
        if (std::ranges::any_of(state.explored,
                                [&](const auto& seen) { return seen.first == dimension && seen.second == at; })) {
            return false;
        }
        return entity::ai::navigation::canReach(context.world, sniffer, at[0] + 0.5, at[1] + 1.0, at[2] + 0.5);
    };
    // parity: `Sniffer.canDig()`, at the square under its nose.
    const auto canDig = [&] {
        const kit::Block head = headBlock(sniffer);
        return state.panicTicks <= 0 && !tempted() && !baby && !inWater && sniffer.onGround &&
               !sniffer.vehicleId.has_value() && canDigAt({head[0], head[1] - 1, head[2]});
    };
    const auto sound = [&](std::string_view name, float pitch) {
        if (!sniffer.sourceNbt.has_value() || !sniffer.sourceNbt->getBoolean("Silent")) {
            playSound(world, sniffer, name, 1.0F, pitch, SoundSource::Neutral);
        }
    };
    // parity: `Sniffer.transitionTo`, each state with its own sound.
    const auto transition = [&](Pose pose) {
        switch (pose) {
        case Pose::FeelingHappy:
            sound("entity.sniffer.happy", 1.0F);
            break;
        case Pose::Scenting:
            sound("entity.sniffer.scenting", baby ? 1.3F : 1.0F);
            break;
        case Pose::Sniffing:
            sound("entity.sniffer.sniffing", 1.0F);
            break;
        case Pose::Digging:
            state.dropSeedAt = sniffer.tickCount + sniffers::kDropSeedOffset;
            entityEvent(sniffer, sniffers::kDiggingEvent);
            break;
        case Pose::Rising:
            sound("entity.sniffer.digging_stop", 1.0F);
            break;
        case Pose::Idling:
        case Pose::Searching:
            break;
        }
        state.pose = pose;
    };
    const auto resetSniffing = [&] {
        state.digging = false;
        state.sniffingTarget.reset();
        transition(Pose::Idling);
    };
    const auto run = [&](Running what, std::int32_t min, std::int32_t max) {
        state.running = what;
        state.runningEnd = now + min + context.random.nextInt(max + 1 - min);
    };
    if (panicking && !wasPanicking) {
        resetSniffing();
    }
    // parity: `updateActivity`: dig, sniff, idle; none valid keeps the last.
    if (!panicking && !state.motion.walk.has_value() && state.digging) {
        state.activity = Activity::Dig;
    } else if (!panicking && state.sniffingTarget.has_value() && state.motion.walk.has_value()) {
        state.activity = Activity::Sniff;
    } else if (!state.digging) {
        state.activity = Activity::Idle;
    }
    switch (state.activity) {
    case Activity::Idle: {
        const bool hadBreedTarget = state.breedTarget.has_value();
        state.breedTarget = kit::makeLove(context, state.motion, state.look, 1.0F, 2);
        if (state.breedTarget.has_value() && !hadBreedTarget) {
            resetSniffing();
        }
        const bool wasFollowing = state.temptation.following;
        static_cast<void>(kit::tempt(
            context, state.motion, state.look, state.temptation, 1.25F,
            [this](const wire::common::Identifier& item) { return itemTagged("minecraft:sniffer_food", item); },
            baby ? 2.5 : 3.5));
        if (state.temptation.following && !wasFollowing) {
            resetSniffing();
        }
        // parity: `FeelingHappy(40, 100)`.
        if (state.happy && !state.feelingHappy) {
            state.feelingHappy = true;
            state.happyEnd = now + 40 + context.random.nextInt(61);
            transition(Pose::FeelingHappy);
        }
        // parity: the idle `RunOne`, held while one of its own runs.
        if (state.running != Running::None) {
            break;
        }
        for (const std::size_t choice : kit::shuffled(context.random, {2, 1, 1, 1, 1, 2})) {
            if (choice == 0 && !state.motion.walk.has_value() &&
                (state.look.entity.has_value() || state.look.at.has_value())) {
                std::optional<kit::Vec> at = state.look.at;
                if (state.look.entity.has_value()) {
                    at = kit::positionOf(context, *state.look.entity, false);
                }
                if (at.has_value()) {
                    kit::walkTo(state.motion, *at, 1.0F, 3);
                }
                break;
            }
            if (choice == 1 && !panicking && !state.digging && !state.sniffingTarget.has_value() && !state.happy &&
                !state.breedTarget.has_value() && !tempted()) {
                run(Running::Scenting, 40, 80);
                transition(Pose::Scenting);
                break;
            }
            if (choice == 2 && !state.motion.walk.has_value() && !state.sniffingTarget.has_value() &&
                !state.sniffCooldown.has(now) && !baby && canSniff()) {
                run(Running::Sniffing, 40, 80);
                transition(Pose::Sniffing);
                break;
            }
            if (choice == 3) {
                // parity: `SetEntityLookTarget(PLAYER, 6)`.
                const std::vector<kit::Seen> seen = kit::nearby(context, 6.0);
                const auto player = std::ranges::find_if(
                    seen, [](const kit::Seen& one) { return one.player != nullptr && one.distanceSquared <= 36.0; });
                if (player != seen.end()) {
                    state.look = kit::Look{.entity = player->id};
                    break;
                }
                continue;
            }
            if (choice == 4 && !state.motion.walk.has_value()) {
                if (const std::optional<kit::Vec> spot = kit::strollSpot(context); spot.has_value()) {
                    kit::walkTo(state.motion, *spot, 1.0F, 0);
                }
                break;
            }
            if (choice == 5) {
                run(Running::Nothing, 5, 20);
                break;
            }
        }
        break;
    }
    case Activity::Sniff:
        // parity: `Searching`, the walk to the scent.
        if (state.running == Running::None && state.motion.walk.has_value() && !panicking &&
            state.sniffingTarget.has_value() && canSniff()) {
            state.running = Running::Searching;
            state.runningEnd = now + 600;
            transition(Pose::Searching);
        }
        break;
    case Activity::Dig:
        // parity: `Digging(160, 180)`, then `FinishedDigging(40)` once the cooldown is on.
        if (state.running == Running::None && !panicking && !state.motion.walk.has_value() && state.digging) {
            if (!state.sniffCooldown.has(now) && canSniff()) {
                run(Running::Digging, 160, 180);
                transition(Pose::Digging);
            } else if (state.sniffCooldown.has(now)) {
                run(Running::FinishedDigging, 40, 40);
                transition(Pose::Rising);
            }
        }
        break;
    }
    snifferRunning(world, context, state, canSniff(), canDig());
    if (state.feelingHappy && now > state.happyEnd) {
        state.feelingHappy = false;
        state.happy = false;
        transition(Pose::Idling);
    }
    // parity: `Sniffer.tick` while digging.
    if (state.pose == Pose::Digging) {
        const kit::Block head = headBlock(sniffer);
        if (sniffer.tickCount % 10 == 0) {
            world::vibrations::emit(world.state(), "entity_action", head[0] + 0.5, head[1] + 0.5, head[2] + 0.5, now,
                                    {.entity = sniffer.uuid()});
        }
        if (state.dropSeedAt == sniffer.tickCount) {
            snifferDropsSeed(world, sniffer, head);
        }
    }
    kit::moveToTarget(context, state.motion);
    kit::lookAtTarget(context, state.look);
}

void MobActionSystem::snifferRunning(world::LoadedWorld& world, entity::ai::MobContext& context, SnifferState& state,
                                     bool canSniff, bool canDig) {
    ServerEntity& sniffer = context.mob;
    const std::int64_t now = context.gameTime;
    const bool timedOut = now > state.runningEnd;
    const auto idle = [&] {
        state.pose = Pose::Idling;
        state.running = Running::None;
    };
    switch (state.running) {
    case Running::None:
        return;
    case Running::Scenting:
    case Running::Nothing:
        if (timedOut) {
            if (state.running == Running::Scenting) {
                idle();
            }
            state.running = Running::None;
        }
        return;
    case Running::Sniffing:
        if (!timedOut && canSniff) {
            return;
        }
        idle();
        // parity: `Sniffing.stop`: only a sniff run to its end finds a spot, a walk of up to five.
        // tries further out each time.
        if (timedOut) {
            for (std::int32_t index = 0; index < 5; ++index) {
                const std::optional<kit::Vec> spot = kit::strollSpot(context, 10 + (2 * index), 3);
                if (!spot.has_value()) {
                    continue;
                }
                const kit::Block below{mth::floor((*spot)[0]), mth::floor((*spot)[1]) - 1, mth::floor((*spot)[2])};
                const std::optional<std::int32_t> stateId = context.world.blockState(below[0], below[1], below[2]);
                const wire::common::Identifier* block =
                    stateId.has_value() ? server::registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
                const bool diggable =
                    block != nullptr && blockTagged("minecraft:sniffer_diggable_block", *block).value_or(false);
                const std::string dimension = config::dimensionRegistryKey(world.declaration().dimension);
                const bool explored = std::ranges::any_of(
                    state.explored, [&](const auto& seen) { return seen.first == dimension && seen.second == below; });
                if (diggable && !explored &&
                    entity::ai::navigation::canReach(context.world, sniffer, below[0] + 0.5, below[1] + 1.0,
                                                     below[2] + 0.5)) {
                    state.sniffingTarget = below;
                    kit::walkTo(state.motion, {below[0] + 0.5, below[1] + 0.5, below[2] + 0.5}, 1.25F, 0);
                    break;
                }
            }
        }
        return;
    case Running::Searching: {
        const bool onTarget = state.motion.walk.has_value() && state.sniffingTarget.has_value() &&
                              kit::Block{mth::floor(state.motion.walk->at[0]), mth::floor(state.motion.walk->at[1]),
                                         mth::floor(state.motion.walk->at[2])} == *state.sniffingTarget;
        if (!canSniff) {
            state.pose = Pose::Idling;
        }
        if (!timedOut && canSniff && onTarget) {
            return;
        }
        state.running = Running::None;
        if (canDig && canSniff) {
            state.digging = true;
        }
        state.motion.walk.reset();
        state.sniffingTarget.reset();
        return;
    }
    case Running::Digging:
        if (!timedOut && state.digging && canDig && sniffer.loveTicks <= 0) {
            return;
        }
        state.running = Running::None;
        if (timedOut) {
            state.sniffCooldown.setFor(true, now, sniffers::kSniffCooldown);
        } else {
            state.digging = false;
            state.sniffingTarget.reset();
            state.pose = Pose::Idling;
        }
        return;
    case Running::FinishedDigging:
        if (!timedOut && state.digging) {
            return;
        }
        idle();
        // parity: `onDiggingComplete`: a dig seen through is remembered where it stands.
        if (timedOut) {
            state.explored.resize(std::min(state.explored.size(), kExploredKept));
            state.explored.insert(state.explored.begin(),
                                  {config::dimensionRegistryKey(world.declaration().dimension),
                                   kit::Block{mth::floor(sniffer.x), mth::floor(sniffer.y - static_cast<double>(0.2F)),
                                              mth::floor(sniffer.z)}});
        }
        state.digging = false;
        state.happy = true;
        return;
    }
}

void MobActionSystem::snifferDropsSeed(world::LoadedWorld& world, const ServerEntity& sniffer,
                                       const std::array<std::int32_t, 3>& head) {
    // parity: `dropSeed` over `dropFromGiftLootTable(SNIFFER_DIGGING)`.
    // corner.
    const server::world::loot::LootTable* table =
        lootTables_->byId(wire::common::Identifier::vanilla("gameplay/sniffer_digging"));
    if (table == nullptr) {
        return;
    }
    random::RandomSource random = freshRandom();
    server::world::loot::LootContext context = deathLootContext(world, sniffer);
    random::RandomSource& roll =
        table->randomSequence.has_value() ? world.randomSequences().of(*table->randomSequence) : random;
    for (const server::world::loot::ItemStack& stack : table->generate(roll, context.luck, std::nullopt, context)) {
        world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                   .uuid = wire::common::Uuid::random(),
                                                   .item = stack.item,
                                                   .count = stack.count,
                                                   .x = static_cast<double>(head[0]),
                                                   .y = static_cast<double>(head[1]),
                                                   .z = static_cast<double>(head[2]),
                                                   .velocityX = (random.nextDouble() * 0.2) - 0.1,
                                                   .velocityY = 0.2,
                                                   .velocityZ = (random.nextDouble() * 0.2) - 0.1,
                                                   .pickupDelay = world::kSpawnedPickupDelay,
                                                   .components = stack.components});
    }
    if (!sniffer.sourceNbt.has_value() || !sniffer.sourceNbt->getBoolean("Silent")) {
        playSound(world, sniffer, "entity.sniffer.drop_seed", 1.0F, 1.0F, SoundSource::Neutral);
    }
}

bool MobActionSystem::pluginSnifferState(ServerEntity& sniffer, std::int32_t pose) {
    world::LoadedWorld* world = worldOf(sniffer);
    if (world == nullptr || sniffer.type().path() != "sniffer" || pose < 0 || pose > static_cast<std::int32_t>(Pose::Rising)) {
        return false;
    }
    SnifferState& state = sniffers::stateOf(sniffer);
    const bool silent = sniffer.sourceNbt.has_value() && sniffer.sourceNbt->getBoolean("Silent");
    const auto sound = [&](std::string_view name, float pitch) {
        if (!silent) {
            playSound(*world, sniffer, name, 1.0F, pitch, SoundSource::Neutral);
        }
    };
    const auto target = static_cast<Pose>(pose);
    // parity: `Sniffer.transitionTo`.
    switch (target) {
    case Pose::FeelingHappy:
        sound("entity.sniffer.happy", 1.0F);
        break;
    case Pose::Scenting:
        sound("entity.sniffer.scenting", sniffer.age < 0 ? 1.3F : 1.0F);
        break;
    case Pose::Sniffing:
        sound("entity.sniffer.sniffing", 1.0F);
        break;
    case Pose::Digging:
        state.dropSeedAt = sniffer.tickCount + sniffers::kDropSeedOffset;
        entityEvent(sniffer, sniffers::kDiggingEvent);
        break;
    case Pose::Rising:
        sound("entity.sniffer.digging_stop", 1.0F);
        break;
    case Pose::Idling:
    case Pose::Searching:
        break;
    }
    state.pose = target;
    return true;
}

namespace {

/// parity: `Sniffer.canDig(BlockPos)`.
[[nodiscard]] bool snifferCanDigAt(MobActionSystem& system, world::LoadedWorld& world, ServerEntity& sniffer,
                                   const kit::Block& at) {
    const std::optional<std::int32_t> stateId = world.state().blockState(at[0], at[1], at[2]);
    const wire::common::Identifier* block =
        stateId.has_value() ? server::registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    if (block == nullptr || !system.blockTagged("minecraft:sniffer_diggable_block", *block).value_or(false)) {
        return false;
    }
    const std::string dimension = config::dimensionRegistryKey(world.declaration().dimension);
    const SnifferState& state = sniffers::stateOf(sniffer);
    if (std::ranges::any_of(state.explored,
                            [&](const auto& seen) { return seen.first == dimension && seen.second == at; })) {
        return false;
    }
    return entity::ai::navigation::canReach(world.state(), sniffer, at[0] + 0.5, at[1] + 1.0, at[2] + 0.5);
}

} // namespace

bool MobActionSystem::pluginSnifferCanDig(ServerEntity& sniffer) {
    world::LoadedWorld* world = worldOf(sniffer);
    if (world == nullptr || sniffer.type().path() != "sniffer") {
        return false;
    }
    const SnifferState& state = sniffers::stateOf(sniffer);
    const kit::Block head = headBlock(sniffer);
    return state.panicTicks <= 0 && !state.temptation.following && sniffer.age >= 0 &&
           !entity::ai::inWater(world->state(), sniffer) && sniffer.onGround && !sniffer.vehicleId.has_value() &&
           snifferCanDigAt(*this, *world, sniffer, {head[0], head[1] - 1, head[2]});
}

std::optional<std::array<std::int32_t, 3>> MobActionSystem::pluginSnifferDigSpot(ServerEntity& sniffer) {
    world::LoadedWorld* world = worldOf(sniffer);
    if (world == nullptr || sniffer.type().path() != "sniffer") {
        return std::nullopt;
    }
    random::RandomSource random = freshRandom();
    // parity: `Sniffer.calculateDigPosition`, five strolls further out each time.
    for (std::int32_t index = 0; index < 5; ++index) {
        const std::optional<kit::Vec> spot =
            entity::ai::randomPos::landPos(world->state(), sniffer, random, 10 + (2 * index), 3);
        if (!spot.has_value()) {
            continue;
        }
        const kit::Block below{mth::floor((*spot)[0]), mth::floor((*spot)[1]) - 1, mth::floor((*spot)[2])};
        if (snifferCanDigAt(*this, *world, sniffer, below)) {
            return below;
        }
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime
