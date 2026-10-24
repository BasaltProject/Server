#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/Allays.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/BeeNavigation.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/RandomPos.hpp>
#include <basalt/server/runtime/player/PlayerParticles.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/ThrownItemTriggers.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using server::world::loot::ItemStack;
using wire::protocol::play::Gamemode;
namespace allays = entity::allays;
namespace nav = entity::ai::beeNavigation;
namespace mth = wire::common::mth;

using Vec = std::array<double, 3>;

/// parity: `Allay.THROW_SOUND_PITCHES`.
constexpr std::array<float, 16> kThrowPitches{0.5625F, 0.625F, 0.75F, 0.9375F, 1.0F, 1.0F, 1.125F, 1.25F,
                                              1.5F,    1.875F, 2.0F,  2.25F,   2.5F, 3.0F, 3.75F,  4.0F};
constexpr double kCloseEnoughToThrow = 3.0;
constexpr double kStayCloseTooFar = 16.0;
constexpr std::int32_t kStayCloseEnough = 4;
constexpr std::int32_t kGiveTimeout = 20;
constexpr std::int32_t kWantedItemRange = 32;
constexpr float kSpeedIdle = 1.0F;
constexpr float kSpeedDeposit = 2.25F;
constexpr float kSpeedRetrieve = 1.75F;
constexpr float kSpeedPanic = 2.5F;
constexpr double kLookRange = 6.0;

[[nodiscard]] double distanceSquared(const Vec& a, const Vec& b) {
    return ((a[0] - b[0]) * (a[0] - b[0])) + ((a[1] - b[1]) * (a[1] - b[1])) + ((a[2] - b[2]) * (a[2] - b[2]));
}

[[nodiscard]] std::array<std::int32_t, 3> blockOf(const Vec& at) {
    return {mth::floor(at[0]), mth::floor(at[1]), mth::floor(at[2])};
}

[[nodiscard]] bool isBlock(const world::LoadedWorld& world, const allays::Pos& at, std::string_view path) {
    const std::optional<std::int32_t> state = world.state().blockState(at[0], at[1], at[2]);
    const wire::common::Identifier* block =
        state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
    return block != nullptr && block->path() == path;
}

/// parity: `ShufflingList.shuffle`.
[[nodiscard]] std::array<std::int32_t, 3> idleOrder(random::RandomSource& random) {
    constexpr std::array<std::int32_t, 3> kWeights{2, 2, 1};
    std::array<std::pair<double, std::int32_t>, 3> drawn{};
    for (std::size_t index = 0; index < drawn.size(); ++index) {
        drawn[index] = {-std::pow(static_cast<double>(random.nextFloat()), 1.0 / kWeights[index]),
                        static_cast<std::int32_t>(index)};
    }
    std::ranges::stable_sort(drawn, {}, &std::pair<double, std::int32_t>::first);
    return {drawn[0].second, drawn[1].second, drawn[2].second};
}

} // namespace

void MobActionSystem::tickAllay(world::LoadedWorld& world, entity::ai::MobContext& context) {
    ServerEntity& allay = context.mob;
    allays::AllayState& state = allays::stateOf(allay);
    random::RandomSource& random = context.random;
    // parity: `AllayAi.getLikedPlayer`.
    state.likedPlayerEntity.reset();
    PlayerSession* liked = nullptr;
    if (state.likedPlayer.has_value()) {
        for (PlayerSession* player : watchersOf(world)) {
            if (player->uuid() == *state.likedPlayer && !player->dead &&
                (player->gamemode == Gamemode::Survival || player->gamemode == Gamemode::Creative)) {
                state.likedPlayerEntity = player->entityId();
                if (distanceSquared({player->x, player->y, player->z}, {allay.x, allay.y, allay.z}) <
                    allays::kLikedPlayerRange * allays::kLikedPlayerRange) {
                    liked = player;
                }
            }
        }
    }
    allayHears(world, allay, state, watchersOf(world));
    // parity: `CountDownCooldownTicks` for both cooldowns, gone once they reach nothing.
    if (state.noteblockCooldown.has_value() && --*state.noteblockCooldown <= 0) {
        state.noteblockCooldown.reset();
    }
    state.pickupCooldown = std::max(state.pickupCooldown - 1, 0);
    // parity: `getItemDepositPosition`.
    // else the liked player's eyes.
    std::optional<Vec> deposit;
    if (state.likedNoteblock.has_value()) {
        const bool here = state.noteblockDimension.empty() ||
                          state.noteblockDimension == config::dimensionRegistryKey(world.declaration().dimension);
        const allays::Pos& block = *state.likedNoteblock;
        const double dx = block[0] - mth::floor(allay.x);
        const double dy = block[1] - mth::floor(allay.y);
        const double dz = block[2] - mth::floor(allay.z);
        if (here && (dx * dx) + (dy * dy) + (dz * dz) < allays::kNoteblockDistance * allays::kNoteblockDistance &&
            isBlock(world, block, "note_block") && state.noteblockCooldown.has_value()) {
            deposit = Vec{block[0] + 0.5, block[1] + 1.5, block[2] + 0.5};
        } else {
            state.likedNoteblock.reset();
        }
    }
    if (!deposit.has_value() && liked != nullptr) {
        deposit = Vec{liked->x, liked->y + static_cast<double>(1.62F), liked->z};
    }
    const std::optional<server::world::loot::ItemStack> hand = entity::copperGolems::mainHand(allay);
    // parity: `NearestItemSensor`.
    // across and sixteen up or down that it can see.
    if (state.itemSensorTicks < 0) {
        state.itemSensorTicks = random.nextInt(20);
    }
    const std::vector<world::DroppedItem> nearItems =
        world.state().nearbyItems(allay.x, allay.y, allay.z, kWantedItemRange + kWantedItemRange);
    if (--state.itemSensorTicks <= 0) {
        state.itemSensorTicks = 20;
        state.wantedItem.reset();
        const double eyes = allay.y + entity::mobBreathing::eyeHeightOf(allay);
        std::vector<const world::DroppedItem*> sorted;
        for (const world::DroppedItem& item : nearItems) {
            if (std::abs(item.x - allay.x) <= kWantedItemRange + 0.2 &&
                std::abs(item.z - allay.z) <= kWantedItemRange + 0.2 &&
                std::abs(item.y - allay.y) <= 16.0 + allay.height()) {
                sorted.push_back(&item);
            }
        }
        std::ranges::sort(sorted, {}, [&allay](const world::DroppedItem* item) {
            return distanceSquared({item->x, item->y, item->z}, {allay.x, allay.y, allay.z});
        });
        for (const world::DroppedItem* item : sorted) {
            const ItemStack stack{.item = item->item, .count = item->count, .components = item->components};
            const bool wants = hand.has_value() && world.gameRules().get(world::gameRules::kMobGriefing) &&
                               allays::considersEqual(*hand, stack) && allays::roomFor(state, stack) > 0;
            const double itemEyes = item->y + static_cast<double>(0.2125F);
            if (wants &&
                distanceSquared({item->x, item->y, item->z}, {allay.x, allay.y, allay.z}) <
                    kWantedItemRange * kWantedItemRange &&
                !entity::blockRay::cast(context.world, allay.x, eyes, allay.z, item->x, itemEyes, item->z)
                     .has_value()) {
                state.wantedItem = item->entityId;
                break;
            }
        }
    }
    std::optional<world::DroppedItem> wanted;
    if (state.wantedItem.has_value()) {
        for (const world::DroppedItem& item : nearItems) {
            if (item.entityId == *state.wantedItem) {
                wanted = item;
            }
        }
    }
    const bool canPickUp = hand.has_value() && state.pickupCooldown == 0;
    // parity: `AnimalPanic(2.5)`, a hundred to a hundred and twenty ticks after a hit.
    if (state.panicTicks == 0 && allay.panicTicks > 0) {
        state.panicTicks = 100 + random.nextInt(21);
        state.walk.reset();
        entity::ai::navigation::stop(allay);
        state.dancing = false;
    }
    if (state.panicTicks > 0) {
        --state.panicTicks;
        if (entity::ai::navigation::isDone(allay)) {
            if (const std::optional<Vec> spot = entity::ai::randomPos::landPos(context.world, allay, random, 5, 4);
                spot.has_value()) {
                state.walk = allays::WalkTarget{.at = *spot, .speed = kSpeedPanic};
            }
        }
    } else {
        // parity: `GoToWantedItem(1.75, interrupt, 32)`.
        if (wanted.has_value() && canPickUp) {
            state.walk = allays::WalkTarget{.at = {wanted->x, wanted->y, wanted->z}, .speed = kSpeedRetrieve};
            state.lookAt = Vec{wanted->x, wanted->y + 0.25, wanted->z};
            state.lookEntity.reset();
        }
        // parity: `GoAndGiveItemsToTarget`.
        const bool canThrow = state.inventory.has_value() && deposit.has_value();
        if (state.giveTicks >= 0 && (!canThrow || state.giveTicks >= kGiveTimeout)) {
            state.giveTicks = -1;
        } else if (state.giveTicks < 0 && canThrow) {
            state.giveTicks = 0;
            state.walk = allays::WalkTarget{.at = *deposit, .speed = kSpeedDeposit, .closeEnough = 3};
            state.lookAt = deposit;
        }
        if (state.giveTicks >= 0 && deposit.has_value()) {
            ++state.giveTicks;
            const Vec eye{allay.x, allay.y + entity::mobBreathing::eyeHeightOf(allay), allay.z};
            if (distanceSquared(*deposit, eye) < kCloseEnoughToThrow * kCloseEnoughToThrow) {
                allayThrows(world, allay, state, *deposit);
                state.pickupCooldown = allays::kPickupCooldown;
            }
        }
        // parity: `StayCloseToTarget(4, 16, 2.25)` while nothing is worth fetching.
        if (deposit.has_value() && !wanted.has_value() &&
            distanceSquared(*deposit, {allay.x, allay.y, allay.z}) >= kStayCloseTooFar * kStayCloseTooFar) {
            state.walk = allays::WalkTarget{.at = *deposit, .speed = kSpeedDeposit, .closeEnough = kStayCloseEnough};
            state.lookAt = deposit;
        }
        // parity: `SetEntityLookTargetSometimes(6, 30..60)`.
        if (!state.lookAt.has_value() && !state.lookEntity.has_value()) {
            const entity::ai::PlayerView* nearest = context.nearestPlayer(kLookRange);
            if (nearest != nullptr) {
                if (state.lookCooldown == 0) {
                    state.lookCooldown = 30 + random.nextInt(31);
                    state.lookEntity = nearest->entityId;
                } else {
                    --state.lookCooldown;
                }
            }
        }
        // parity: the idle `RunOne`.
        // of nothing.
        if (state.idleTicks > 0) {
            --state.idleTicks;
        } else {
            for (const std::int32_t choice : idleOrder(random)) {
                if (choice == 0 && !state.walk.has_value()) {
                    const std::optional<Vec> spot = nav::airAndWaterSpot(context, 10, 7, -2);
                    if (spot.has_value()) {
                        state.walk = allays::WalkTarget{.at = *spot, .speed = kSpeedIdle};
                    }
                    break;
                }
                if (choice == 1 && !state.walk.has_value() &&
                    (state.lookAt.has_value() || state.lookEntity.has_value())) {
                    Vec at = state.lookAt.value_or(Vec{});
                    if (state.lookEntity.has_value()) {
                        if (const entity::ai::PlayerView* player = context.playerById(*state.lookEntity);
                            player != nullptr) {
                            at = {player->x, player->y, player->z};
                        }
                    }
                    state.walk = allays::WalkTarget{.at = at, .speed = kSpeedIdle, .closeEnough = 3};
                    break;
                }
                if (choice == 2) {
                    state.idleTicks = 30 + random.nextInt(31);
                    break;
                }
            }
        }
    }
    // parity: `LookAtTargetSink`.
    if (state.lookEntity.has_value()) {
        if (const entity::ai::PlayerView* player = context.playerById(*state.lookEntity); player != nullptr) {
            entity::ai::lookControl::lookAt(allay, player->x, player->y + static_cast<double>(1.62F), player->z);
        } else {
            state.lookEntity.reset();
        }
        if (++state.lookTicks > 90) {
            state.lookEntity.reset();
            state.lookTicks = 0;
        }
    } else if (state.lookAt.has_value()) {
        entity::ai::lookControl::lookAt(allay, (*state.lookAt)[0], (*state.lookAt)[1], (*state.lookAt)[2]);
        if (++state.lookTicks > 90) {
            state.lookAt.reset();
            state.lookTicks = 0;
        }
    }
    // parity: `MoveToTargetSink`.
    if (state.walk.has_value()) {
        const std::array<std::int32_t, 3> target = blockOf(state.walk->at);
        const std::array<std::int32_t, 3> own{mth::floor(allay.x), mth::floor(allay.y), mth::floor(allay.z)};
        const std::int32_t manhattan =
            std::abs(target[0] - own[0]) + std::abs(target[1] - own[1]) + std::abs(target[2] - own[2]);
        const bool moved = !state.lastPathTarget.has_value() ||
                           distanceSquared({static_cast<double>(target[0]), static_cast<double>(target[1]),
                                            static_cast<double>(target[2])},
                                           *state.lastPathTarget) > 4.0;
        if (manhattan <= state.walk->closeEnough) {
            state.walk.reset();
            state.lastPathTarget.reset();
            entity::ai::navigation::stop(allay);
        } else if (state.lastPathTarget.has_value() && entity::ai::navigation::isDone(allay) && !moved) {
            state.walk.reset();
            state.lastPathTarget.reset();
        } else if (moved) {
            const nav::Route route = nav::plan(context, target, 0);
            if (route.nodes.empty()) {
                state.walk.reset();
                state.lastPathTarget.reset();
            } else {
                static_cast<void>(nav::follow(context, route, static_cast<double>(state.walk->speed)));
                state.lastPathTarget =
                    Vec{static_cast<double>(target[0]), static_cast<double>(target[1]), static_cast<double>(target[2])};
            }
        }
    }
    // parity: `Swim(0.8)`, a kick upwards in water four times in five.
    if (entity::ai::inWater(context.world, allay) && random.nextFloat() < 0.8F) {
        allay.velocityY += 0.04;
    }
    // parity: `Mob.aiStep`'s pickup, then the rest of `Allay.aiStep`.
    if (canPickUp && !allay.dying && world.gameRules().get(world::gameRules::kMobGriefing)) {
        allayPicksUp(world, allay, state);
    }
    if (!allay.dying && allay.tickCount % 10 == 0) {
        allay.health = std::min(allay.health + 1.0, allay.attributeWithModifiers("minecraft:max_health"));
    }
    if (state.dancing && !state.forceDancing && allay.tickCount % 20 == 0 &&
        (!state.jukebox.has_value() ||
         distanceSquared({(*state.jukebox)[0] + 0.5, (*state.jukebox)[1] + 0.5, (*state.jukebox)[2] + 0.5},
                         {allay.x, allay.y, allay.z}) >= allays::kJukeboxRadius * allays::kJukeboxRadius ||
         !isBlock(world, *state.jukebox, "jukebox"))) {
        state.dancing = false;
        state.jukebox.reset();
    }
    state.duplicationCooldown = std::max<std::int64_t>(state.duplicationCooldown - 1, 0);
}

/// parity: `VibrationSystem.Ticker` sending the vibration particle toward the allay; whether a player got it.
[[nodiscard]] bool allayVibrationParticle(const std::vector<PlayerSession*>& watchers, const ServerEntity& allay,
                                          double eyeY, const allays::AllayState::Vibration& vibration,
                                          std::int32_t arrivalTicks, double alpha) {
    const server::registry::ParticleType* type =
        server::registry::particleTypes::of(wire::common::Identifier::vanilla("vibration"));
    if (type == nullptr) {
        return false;
    }
    double x = vibration.x;
    double y = vibration.y;
    double z = vibration.z;
    if (alpha > 0.0) {
        x += ((allay.x - vibration.x) * alpha);
        y += ((eyeY - vibration.y) * alpha);
        z += ((allay.z - vibration.z) * alpha);
    }
    const wire::protocol::play::LevelParticlesPacket packet{
        .particleTypeId = type->protocolId,
        .x = x,
        .y = y,
        .z = z,
        .count = 1,
        .options = wire::protocol::play::ParticleOptions{wire::protocol::play::ParticleOptions::Vibration{
            wire::protocol::play::ParticleOptions::Vibration::EntityTarget{allay.entityId(),
                                                                           static_cast<float>(eyeY - allay.y)},
            arrivalTicks}}};
    constexpr double kParticleRange = 32.0;
    bool anyone = false;
    for (const PlayerSession* watcher : watchers) {
        const double dx = watcher->x - x;
        const double dy = watcher->y - y;
        const double dz = watcher->z - z;
        if ((dx * dx) + (dy * dy) + (dz * dz) >= kParticleRange * kParticleRange) {
            continue;
        }
        player::showParticleFor(*watcher, "minecraft:vibration", packet);
        anyone = true;
    }
    return anyone;
}

void MobActionSystem::allayHears(world::LoadedWorld& world, ServerEntity& allay, allays::AllayState& state,
                                 const std::vector<PlayerSession*>& watchers) {
    const std::int64_t now = world.gameTime();
    const double eyeY = allay.y + entity::mobBreathing::eyeHeightOf(allay);
    std::int64_t lastTime = state.heardTime;
    std::size_t countAtLast = 0;
    for (const world::GameEventRecord& record : world.state().recentGameEvents()) {
        if (record.gameTime < state.heardTime) {
            continue;
        }
        const std::size_t seen = record.gameTime == lastTime ? ++countAtLast : (countAtLast = 1);
        lastTime = record.gameTime;
        if (record.gameTime == state.heardTime && seen <= state.heardCount) {
            continue;
        }
        const allays::Pos pos{mth::floor(record.x), mth::floor(record.y), mth::floor(record.z)};
        const double dx = record.x - allay.x;
        const double dz = record.z - allay.z;
        // parity: `JukeboxListener`, ten blocks round the allay's feet.
        if (record.event == "jukebox_play" || record.event == "jukebox_stop_play") {
            const double dy = record.y - allay.y;
            if ((dx * dx) + (dy * dy) + (dz * dz) > allays::kJukeboxRadius * allays::kJukeboxRadius) {
                continue;
            }
            if (record.event == "jukebox_play") {
                // parity: `setDancing` refuses a panicking allay.
                if (!state.dancing && state.panicTicks == 0 && !allay.noAi) {
                    state.jukebox = pos;
                    state.dancing = true;
                }
            } else if (!state.jukebox.has_value() || *state.jukebox == pos) {
                state.jukebox.reset();
                state.dancing = false;
            }
            continue;
        }
        // parity: the vibration user: deaf while one travels, sixteen blocks from its eyes, past no.
        // wool, and only the liked note block once it has one.
        const double dy = record.y - eyeY;
        const bool liked = !state.likedNoteblock.has_value() ||
                           (*state.likedNoteblock == pos &&
                            (state.noteblockDimension.empty() ||
                             state.noteblockDimension == config::dimensionRegistryKey(world.declaration().dimension)));
        if (state.vibrationTravelling.has_value() || record.event != "note_block_play" || allay.noAi || !liked ||
            (dx * dx) + (dy * dy) + (dz * dz) > allays::kListenerRadius * allays::kListenerRadius ||
            world::vibrations::occludedBetween(world.state(), record.x, record.y, record.z, allay.x, eyeY, allay.z)) {
            continue;
        }
        // parity: `VibrationSelector`.
        const auto distance = static_cast<float>(std::sqrt((dx * dx) + (dy * dy) + (dz * dz)));
        if (!state.vibrationCandidate.has_value() ||
            (state.vibrationCandidate->tick == record.gameTime && distance < state.vibrationCandidate->distance)) {
            state.vibrationCandidate = allays::AllayState::Vibration{
                .pos = pos, .x = record.x, .y = record.y, .z = record.z, .distance = distance, .tick = record.gameTime};
        }
    }
    // parity: `VibrationSystem.Ticker.tick`.
    if (!state.vibrationTravelling.has_value() && state.vibrationCandidate.has_value() &&
        state.vibrationCandidate->tick < now) {
        state.vibrationTravelling = std::exchange(state.vibrationCandidate, std::nullopt);
        state.vibrationTicks = mth::floor(static_cast<double>(state.vibrationTravelling->distance));
        // parity: `trySelectAndScheduleVibration` arms the reload until the particle reaches a player.
        state.vibrationParticlePending =
            !allayVibrationParticle(watchers, allay, eyeY, *state.vibrationTravelling, state.vibrationTicks, 0.0);
    }
    if (state.vibrationTravelling.has_value()) {
        if (state.vibrationParticlePending) {
            // parity: `tryReloadVibrationParticle` resends from the lerped origin until someone sees it.
            const std::int32_t initial = mth::floor(static_cast<double>(state.vibrationTravelling->distance));
            const double alpha =
                initial > 0 ? 1.0 - (static_cast<double>(state.vibrationTicks) / static_cast<double>(initial)) : 0.0;
            state.vibrationParticlePending =
                !allayVibrationParticle(watchers, allay, eyeY, *state.vibrationTravelling, state.vibrationTicks, alpha);
        }
        state.vibrationTicks = std::max(0, state.vibrationTicks - 1);
        if (state.vibrationTicks <= 0) {
            state.vibrationParticlePending = false;
            // parity: `AllayAi.hearNoteblock`.
            const allays::Pos pos = std::exchange(state.vibrationTravelling, std::nullopt)->pos;
            if (!state.likedNoteblock.has_value()) {
                state.likedNoteblock = pos;
                state.noteblockDimension = config::dimensionRegistryKey(world.declaration().dimension);
                state.noteblockCooldown = allays::kNoteblockCooldown;
            } else if (*state.likedNoteblock == pos) {
                state.noteblockCooldown = allays::kNoteblockCooldown;
            }
        }
    }
    if (lastTime > state.heardTime || (lastTime == state.heardTime && countAtLast > state.heardCount)) {
        state.heardTime = lastTime;
        state.heardCount = countAtLast;
    }
}

void MobActionSystem::allayThrows(world::LoadedWorld& world, ServerEntity& allay, allays::AllayState& state,
                                  const Vec& target) {
    if (!state.inventory.has_value() || state.inventory->count <= 0) {
        return;
    }
    ItemStack thrown = *state.inventory;
    thrown.count = 1;
    if (--state.inventory->count <= 0) {
        state.inventory.reset();
    }
    // parity: `BehaviorUtils.throwItem` at the target a block up.
    const double tx = target[0] - allay.x;
    const double ty = target[1] + 1.0 - allay.y;
    const double tz = target[2] - allay.z;
    const double length = std::sqrt((tx * tx) + (ty * ty) + (tz * tz));
    const double scale = length < 1.0E-4 ? 0.0 : 1.0 / length;
    world::DroppedItem dropped{.entityId = players_->allocateEntityId(),
                               .uuid = wire::common::Uuid::random(),
                               .item = thrown.item,
                               .count = 1,
                               .x = allay.x,
                               .y = allay.y + entity::mobBreathing::eyeHeightOf(allay) - static_cast<double>(0.2F),
                               .z = allay.z,
                               .components = thrown.components};
    dropped.velocityX = tx * scale * static_cast<double>(0.2F);
    dropped.velocityY = ty * scale * static_cast<double>(0.3F);
    dropped.velocityZ = tz * scale * static_cast<double>(0.2F);
    dropped.pickupDelay = world::kSpawnedPickupDelay;
    dropped.thrower = allay.uuid();
    world.state().spawnItem(std::move(dropped));
    // parity: `AllayAi.throwItem` tells the liked player.
    const player::PlayerSession* liked = state.likedPlayer.has_value() ? players_->get(*state.likedPlayer) : nullptr;
    if (liked != nullptr && options_.advancementTriggers != nullptr && lootTables_ != nullptr &&
        liked->worldName == world.name() &&
        (liked->gamemode == Gamemode::Survival || liked->gamemode == Gamemode::Creative) &&
        ((liked->x - allay.x) * (liked->x - allay.x)) + ((liked->y - allay.y) * (liked->y - allay.y)) +
                ((liked->z - allay.z) * (liked->z - allay.z)) <
            allays::kLikedPlayerRange * allays::kLikedPlayerRange) {
        const storage::StoredItemStack item{.id = thrown.item.toString(), .count = 1, .components = thrown.components};
        options_.advancementTriggers->fire(
            "allay_drop_item_on_block", liked->name(), liked->uuid().toString(),
            world::TriggerEvent{
                .location = world::triggerFacts::atBlock(*lootTables_, world, *liked, mth::floor(target[0]),
                                                         mth::floor(target[1]) - 1, mth::floor(target[2]), item)});
    }
    random::RandomSource random = freshRandom();
    if (world.gameTime() % 7 == 0 && random.nextDouble() < 0.9) {
        const float pitch = kThrowPitches[static_cast<std::size_t>(random.nextInt(kThrowPitches.size()))];
        playSound(world, allay, "entity.allay.throw", 1.0F, pitch);
    }
}

void MobActionSystem::allayPicksUp(world::LoadedWorld& world, ServerEntity& allay, allays::AllayState& state) {
    const std::optional<ItemStack> hand = entity::copperGolems::mainHand(allay);
    if (!hand.has_value()) {
        return;
    }
    // parity: `Allay.ITEM_PICKUP_REACH`, a block every way round its box.
    const double half = allay.width() / 2.0;
    for (const world::DroppedItem& item :
         world.state().nearbyItems(allay.x, allay.y, allay.z, 3.0 + allay.width() + allay.height())) {
        constexpr double kItemHalf = 0.125;
        if (!item.canBePickedUp() || item.count <= 0 || item.x + kItemHalf < allay.x - half - 1.0 ||
            item.x - kItemHalf > allay.x + half + 1.0 || item.z + kItemHalf < allay.z - half - 1.0 ||
            item.z - kItemHalf > allay.z + half + 1.0 || item.y + 0.25 < allay.y - 1.0 ||
            item.y > allay.y + allay.height() + 1.0) {
            continue;
        }
        const ItemStack stack{.item = item.item, .count = item.count, .components = item.components};
        const std::int32_t room = allays::roomFor(state, stack);
        if (room <= 0 || !allays::considersEqual(*hand, stack)) {
            continue;
        }
        // parity: `InventoryCarrier.pickUpItem`, the thrower told before the stack is split.
        world::thrownItems::pickedUpByMob(options_.advancementTriggers, world, watchersOf(world), item, item.count,
                                          allay);
        if (state.inventory.has_value()) {
            state.inventory->count += room;
        } else {
            state.inventory = ItemStack{.item = item.item, .count = room, .components = item.components};
        }
        if (room >= item.count) {
            static_cast<void>(world.state().removeItem(item.entityId));
        } else {
            static_cast<void>(world.state().setItemCount(item.entityId, item.count - room));
        }
        const wire::protocol::play::TakeItemEntityPacket packet{
            .collectedEntityId = item.entityId, .collectorEntityId = allay.entityId(), .pickupItemCount = room};
        for (PlayerSession* player : watchersOf(world)) {
            if (player->connection().sendPlayPacket) {
                player->connection().sendPlayPacket(wire::protocol::play::TakeItemEntityPacket::kId,
                                                    [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
        }
    }
}

MobActionSystem::AllayUse MobActionSystem::useOnAllay(world::LoadedWorld& world, PlayerSession& player,
                                                      ServerEntity& allay, const std::optional<ItemStack>& held,
                                                      bool mainHand) {
    allays::AllayState& state = allays::stateOf(allay);
    const std::optional<ItemStack> hand = entity::copperGolems::mainHand(allay);
    // parity: an amethyst shard to a dancing allay that may duplicate.
    if (state.dancing && held.has_value() &&
        blockTagged("minecraft:duplicates_allays", held->item).value_or(held->item.path() == "amethyst_shard") &&
        state.duplicationCooldown == 0) {
        if (spawning_ != nullptr) {
            static_cast<void>(spawning_->spawnMob(
                world, allay.type(), allay.x, allay.y, allay.z, allay.yaw, "breeding", [](ServerEntity& copy) {
                    copy.persistent = true;
                    allays::stateOf(copy).duplicationCooldown = allays::kDuplicationCooldown;
                }));
        }
        state.duplicationCooldown = allays::kDuplicationCooldown;
        entityEvent(allay, 18);
        playSound(world, allay, "block.amethyst_block.chime", 2.0F, 1.0F);
        return AllayUse::ConsumeOne;
    }
    if (!hand.has_value() && held.has_value() && held->count > 0) {
        ItemStack given = *held;
        given.count = 1;
        entity::copperGolems::setMainHand(allay, given, true);
        heldItemChanged(allay);
        playSound(world, allay, "entity.allay.item_given", 2.0F, 1.0F);
        state.likedPlayer = player.uuid();
        return AllayUse::ConsumeOne;
    }
    if (hand.has_value() && mainHand && !held.has_value()) {
        entity::copperGolems::setMainHand(allay, std::nullopt, false);
        heldItemChanged(allay);
        playSound(world, allay, "entity.allay.item_taken", 2.0F, 1.0F);
        swing(allay);
        // parity: `BehaviorUtils.throwItem(this, stack, position())`, which throws with no speed.
        if (state.inventory.has_value()) {
            world::DroppedItem dropped{.entityId = players_->allocateEntityId(),
                                       .uuid = wire::common::Uuid::random(),
                                       .item = state.inventory->item,
                                       .count = state.inventory->count,
                                       .x = allay.x,
                                       .y = allay.y + entity::mobBreathing::eyeHeightOf(allay) - 0.3,
                                       .z = allay.z,
                                       .components = state.inventory->components};
            dropped.pickupDelay = world::kSpawnedPickupDelay;
            dropped.thrower = allay.uuid();
            world.state().spawnItem(std::move(dropped));
            state.inventory.reset();
        }
        state.likedPlayer.reset();
        state.likedPlayerEntity.reset();
        const std::int32_t leftover = player.inventory.add(storage::StoredItemStack{
            .id = hand->item.toString(), .count = hand->count, .components = hand->components});
        if (leftover > 0) {
            world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                       .uuid = wire::common::Uuid::random(),
                                                       .item = hand->item,
                                                       .count = leftover,
                                                       .x = player.x,
                                                       .y = player.y + 1.32,
                                                       .z = player.z,
                                                       .pickupDelay = 40,
                                                       .components = hand->components});
        }
        return AllayUse::Done;
    }
    return AllayUse::Pass;
}

void MobActionSystem::dropAllayInventory(world::LoadedWorld& world, const ServerEntity& allay) {
    if (allay.allay == nullptr || !allay.allay->inventory.has_value()) {
        return;
    }
    random::RandomSource random = freshRandom();
    const ItemStack& stack = *allay.allay->inventory;
    // parity: `Entity.spawnAtLocation`, thrown up and a little aside.
    world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                               .uuid = wire::common::Uuid::random(),
                                               .item = stack.item,
                                               .count = stack.count,
                                               .x = allay.x,
                                               .y = allay.y,
                                               .z = allay.z,
                                               .velocityX = (random.nextDouble() * 0.2) - 0.1,
                                               .velocityY = 0.2,
                                               .velocityZ = (random.nextDouble() * 0.2) - 0.1,
                                               .pickupDelay = world::kSpawnedPickupDelay,
                                               .components = stack.components});
}

} // namespace basalt::server::runtime
