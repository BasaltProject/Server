#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ChunkStreamer.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/EntityVisibilityTracker.hpp>
#include <basalt/server/runtime/ItemUseTimers.hpp>
#include <basalt/server/runtime/PlayerLifecycle.hpp>
#include <basalt/server/runtime/UiStateSync.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerImpulse.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerPersistence.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/PlayerAdvancements.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/Component.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <utility>
#include <vector>

namespace basalt::server::runtime {
namespace {

/// parity: `Avatar.DEFAULT_VEHICLE_ATTACHMENT`, where a player sits on what carries them.
constexpr double kPlayerVehicleAttachment = 0.6;

using player::PlayerSession;
using wire::protocol::play::Gamemode;
using wire::text::Component;

std::int64_t nowEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace

PlayerLifecycle::PlayerLifecycle(ClientSession& owner, Hooks hooks) : owner_(&owner), hooks_(std::move(hooks)) {}

void PlayerLifecycle::checkEquipment(player::PlayerSession& session) {
    std::array<std::optional<storage::StoredItemStack>, 4> worn;
    for (std::int32_t slot = 0; slot < 4; ++slot) {
        worn[static_cast<std::size_t>(slot)] = session.inventory.at(player::PlayerInventory::kArmourStart + slot);
    }
    const std::optional<std::array<std::optional<storage::StoredItemStack>, 4>> before = worn_;
    worn_ = worn;
    if (!before.has_value() || session.gamemode == Gamemode::Spectator) {
        return;
    }
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    for (std::size_t slot = 0; slot < worn.size(); ++slot) {
        if ((*before)[slot] == worn[slot]) {
            continue;
        }
        // parity: `equippable`.
        world::vibrations::emit(world->state(), worn[slot].has_value() ? "equip" : "unequip", session.x, session.y,
                                session.z, world->gameTime(), {.entity = session.uuid()});
    }
}

void PlayerLifecycle::tickPlayer() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    // parity: `LivingEntity.aiStep` counts a riptide's spin down, and it stops at nothing.
    if (session->autoSpinTicks > 0 && --session->autoSpinTicks <= 0) {
        session->autoSpinDamage = 0.0F;
        session->autoSpinItem.reset();
    }
    // parity: `ServerPlayer.tick`'s five clocks, in its order.
    owner_->playTimeTicks++;
    session->stats.noteOnly(storage::stats::playTime(), 1);
    player::statAwards::award(*session, "total_world_time");
    if (!session->dead) {
        player::statAwards::award(*session, "time_since_death");
    }
    if (session->sneaking) {
        player::statAwards::award(*session, "sneak_time");
    }
    if (!session->sleepingBed.has_value()) {
        session->stats.increment(storage::stats::timeSinceRest());
    }
    checkAdvancements(*session);
    tickTriggers(*session);
    if (session->dead) {
        return;
    }
    rideTick(*session);
    owner_->tryPickupNearbyItems();
    checkEquipment(*session);
    owner_->uiState().syncHeldAttributes();
    owner_->itemUseTimers().tickUsingTrigger(*session);
    owner_->itemUseTimers().tickEating(*session);
    owner_->itemUseTimers().tickDrawing(*session);
    owner_->itemUseTimers().tickBlocking(*session);
    owner_->combat().tickCharge(*session);
    owner_->itemUseTimers().tickSpyglass(*session);
    owner_->blockInteraction().tickDigging(*session);
}

void PlayerLifecycle::fireEffectsChanged(PlayerSession& session, world::AdvancementTriggers& triggers) {
    const std::string name{session.name()};
    const std::string uuid = session.uuid().toString();
    // parity: `ServerPlayer.onEffectAdded`.
    std::vector<std::optional<std::int32_t>> sources = session.effectSources;
    std::ranges::sort(sources);
    sources.erase(std::ranges::unique(sources).begin(), sources.end());
    if (sources.empty()) {
        sources.emplace_back(std::nullopt);
    }
    world::LoadedWorld* world = owner_->currentWorld();
    player::PlayerRegistry* registry = owner_->options().playerRegistry;
    for (const std::optional<std::int32_t>& source : sources) {
        world::TriggerEvent event;
        if (source.has_value() && world != nullptr) {
            std::optional<server::world::loot::EntityFacts> facts = world::triggerFacts::ofId(
                *world, registry != nullptr ? registry->snapshot() : std::vector<PlayerSession*>{}, *source);
            if (facts.has_value()) {
                event.entities.emplace("source", std::move(*facts));
            }
        }
        triggers.fire("effects_changed", name, uuid, event);
    }
}

void PlayerLifecycle::tickTriggers(PlayerSession& session) {
    world::AdvancementTriggers* triggers = owner_->options().advancementTriggers;
    if (triggers == nullptr) {
        return;
    }
    const std::string name{session.name()};
    const std::string uuid = session.uuid().toString();
    bool changed = session.effects.size() != seenEffects_.size();
    std::map<std::string, std::pair<std::int32_t, std::int32_t>, std::less<>> now;
    for (const auto& [id, active] : session.effects) {
        const std::string key = id.toString();
        const auto before = seenEffects_.find(key);
        // parity: a refresh or a stronger dose is `onEffectUpdated`.
        changed = changed || before == seenEffects_.end() || before->second.first != active.amplifier ||
                  (active.remainingTicks > before->second.second && !active.infinite());
        now.emplace(key, std::pair{active.amplifier, active.remainingTicks});
    }
    const bool levitating = now.contains("minecraft:levitation");
    if (levitating && !seenEffects_.contains("minecraft:levitation")) {
        levitationStart_ = std::array<double, 3>{session.x, session.y, session.z};
        levitationTicks_ = 0;
    } else if (!levitating) {
        levitationStart_.reset();
    }
    seenEffects_ = std::move(now);
    if (changed) {
        fireEffectsChanged(session, *triggers);
    }
    session.effectSources.clear();
    triggers->fire("tick", name, uuid);
    // parity: `trackEnteredOrExitedLavaOnVehicle`.
    world::LoadedWorld* world = owner_->currentWorld();
    const entity::ServerEntity* vehicle =
        world == nullptr || !session.vehicleId.has_value() ? nullptr : world->state().entity(*session.vehicleId);
    const bool inLava = vehicle != nullptr && world::fluids::touchesLava(world->state(), *vehicle);
    if (inLava) {
        if (!enteredLavaOnVehicle_.has_value()) {
            enteredLavaOnVehicle_ = std::array<double, 3>{session.x, session.y, session.z};
        } else {
            triggers->fire("ride_entity_in_lava", name, uuid,
                           world::TriggerEvent{.startPosition = enteredLavaOnVehicle_});
        }
    } else {
        enteredLavaOnVehicle_.reset();
    }
    if (levitationStart_.has_value()) {
        triggers->fire(
            "levitation", name, uuid,
            world::TriggerEvent{.numbers = {{"duration", levitationTicks_}}, .startPosition = levitationStart_});
        ++levitationTicks_;
    }
}

void PlayerLifecycle::rideTick(PlayerSession& session) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (!session.vehicleId.has_value() || world == nullptr) {
        return;
    }
    const entity::ServerEntity* vehicle = world->state().entity(*session.vehicleId);
    if (vehicle == nullptr || vehicle->removed) {
        return;
    }
    // parity: `positionRider`.
    const double x = vehicle->x;
    const double y = vehicle->y + vehicle->height() - kPlayerVehicleAttachment;
    const double z = vehicle->z;
    player::statAwards::awardRiding(session, vehicle->type(), x - session.x, y - session.y, z - session.z);
    session.x = x;
    session.y = y;
    session.z = z;
    owner_->chunkStreamer().streamChunksAround(x, z);
}

void PlayerLifecycle::checkAdvancements(PlayerSession& session) {
    world::AdvancementTriggers* triggers = owner_->options().advancementTriggers;
    // parity: `ServerPlayer.inventoryMenu.broadcastChanges`.
    if (triggers == nullptr || !session.inventory.changed()) {
        return;
    }
    session.inventory.clearChanged();
    triggers->onInventoryChanged(session.name(), session.uuid().toString(), session.inventory.snapshot().slots);
}

void PlayerLifecycle::respawn() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    // parity: `handleClientCommand`, where the credits end by asking to respawn alive.
    if (session->wonGame && session->health() > 0.0F) {
        session->wonGame = false;
        const auto found = owner_->options().worlds.find(session->creditsExitWorld);
        if (found != owner_->options().worlds.end() && session->connection().switchWorld) {
            static_cast<void>(session->connection().switchWorld(*found->second, session->creditsExit[0],
                                                                session->creditsExit[1], session->creditsExit[2]));
        }
        return;
    }
    if (session->health() > 0.0F) {
        return;
    }
    // parity: `ServerPlayer.findRespawnPositionAndUseSpawnBlock`.
    storage::PlayerStore* store = owner_->options().playerStore;
    const std::optional<storage::StoredSpawnPoint> stored =
        store != nullptr && store->isOpen() ? store->access().spawnPointOf(session->uuid().toString()) : std::nullopt;
    world::LoadedWorld* respawnWorld = owner_->currentWorld();
    if (stored.has_value()) {
        const auto found = owner_->options().worlds.find(stored->world);
        if (found != owner_->options().worlds.end()) {
            respawnWorld = found->second;
        }
    }
    if (!stored.has_value() && owner_->options().respawnWorld) {
        if (world::LoadedWorld* configured = owner_->options().respawnWorld(); configured != nullptr) {
            respawnWorld = configured;
        }
    }
    if (respawnWorld == nullptr) {
        return;
    }
    double spawnX = stored.has_value() ? stored->x : respawnWorld->spawn()[0];
    double spawnY = stored.has_value() ? stored->y : respawnWorld->spawn()[1];
    double spawnZ = stored.has_value() ? stored->z : respawnWorld->spawn()[2];
    session->resetForRespawn();
    session->x = spawnX;
    session->y = spawnY;
    session->z = spawnZ;
    if (hooks_.respawned) {
        hooks_.respawned(*session, *respawnWorld, spawnX, spawnY, spawnZ);
        session->x = spawnX;
        session->y = spawnY;
        session->z = spawnZ;
    }

    owner_->entityTracker().forgetEverythingDrawn();
    owner_->sendCompressed(wire::protocol::play::RespawnPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::RespawnPacket{.spawnInfo = owner_->spawnInfo(session->gamemode), .dataToKeep = 0}.write(
            buffer);
    });
    owner_->uiState().forgetSentAttributes();
    owner_->sendPlayerPosition(session->x, session->y, session->z, session->yaw, session->pitch);
    // parity: `PlayerList.sendLevelInfo` again, which is what a respawn goes through too.
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        owner_->sendCompressed(
            wire::protocol::play::InitializeBorderPacket::kId,
            [world](wire::io::ByteBuffer& buffer) { world->border().initializePacket().write(buffer); });
        owner_->uiState().sendTime(*world);
        if (world->weather().raining) {
            owner_->uiState().sendWeather(world->weather());
        }
    }
    owner_->sendCompressed(wire::protocol::play::SpawnPositionPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::SpawnPositionPacket{
            .globalPos =
                wire::protocol::play::GlobalPos{.dimensionName = respawnWorld->levelKey(),
                                                .location =
                                                    wire::protocol::play::BlockPos{static_cast<std::int32_t>(spawnX),
                                                                                   static_cast<std::int32_t>(spawnY),
                                                                                   static_cast<std::int32_t>(spawnZ)}},
            .yaw = 0.0F,
            .pitch = 0.0F}
            .write(buffer);
    });
    owner_->uiState().sendAbilities();
    owner_->uiState().syncHealth();
    owner_->uiState().syncExperience();
    // parity: `PlayerList.respawn` carries the post effects over and sends them again.
    owner_->sendPostEffects();

    owner_->chunkStreamer().resetForRespawn(spawnX, spawnZ);

    // parity: `ServerGamePacketListenerImpl.handleClientCommand` puts a hardcore player in spectator after the respawn.
    const bool hardcore = respawnWorld->hardcore();
    if (respawnWorld != owner_->currentWorld() && session->connection().switchWorld) {
        static_cast<void>(session->connection().switchWorld(*respawnWorld, spawnX, spawnY, spawnZ));
        if (hardcore) {
            owner_->setGamemode(wire::protocol::play::Gamemode::Spectator, "hardcore_death");
        }
        return;
    }

    for (PlayerSession* other : owner_->playersInSameWorld()) {
        if (other->uuid() == session->uuid() || !other->connection().sendPlayPacket) {
            continue;
        }
        other->connection().sendPlayPacket(
            wire::protocol::play::EntityTeleportPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                wire::protocol::play::EntityTeleportPacket{.entityId = session->entityId(),
                                                           .x = spawnX,
                                                           .y = spawnY,
                                                           .z = spawnZ,
                                                           .yaw = 0.0F,
                                                           .pitch = 0.0F,
                                                           .onGround = true}
                    .write(buffer);
            });
    }
    if (hardcore) {
        owner_->setGamemode(wire::protocol::play::Gamemode::Spectator, "hardcore_death");
    }
}

void PlayerLifecycle::refreshProfile() {
    PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || world == nullptr || session->dead) {
        return;
    }
    const wire::protocol::play::PlayerInfoEntry entry{.uuid = session->uuid(),
                                                      .name = session->name(),
                                                      .listed = true,
                                                      .gamemode = session->gamemode,
                                                      .properties = session->profileProperties,
                                                      .chatSession = session->chatSession()};
    owner_->sendCompressed(wire::protocol::play::PlayerInfoRemovePacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::PlayerInfoRemovePacket{.players = {session->uuid()}}.write(buffer);
    });
    owner_->sendCompressed(wire::protocol::play::PlayerInfoUpdatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::PlayerInfoUpdatePacket::initializing({entry}).write(buffer);
    });
    // parity: `ServerPlayer.refreshPlayer`'s respawn in place keeps the data of the player and loses the world.
    owner_->entityTracker().forgetEverythingDrawn();
    owner_->sendCompressed(wire::protocol::play::RespawnPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::RespawnPacket{.spawnInfo = owner_->spawnInfo(session->gamemode), .dataToKeep = 3}.write(
            buffer);
    });
    owner_->uiState().forgetSentAttributes();
    owner_->sendPlayerPosition(session->x, session->y, session->z, session->yaw, session->pitch);
    owner_->sendCompressed(wire::protocol::play::InitializeBorderPacket::kId,
                           [world](wire::io::ByteBuffer& buffer) { world->border().initializePacket().write(buffer); });
    owner_->uiState().sendTime(*world);
    if (world->weather().raining) {
        owner_->uiState().sendWeather(world->weather());
    }
    owner_->uiState().sendAbilities();
    owner_->uiState().syncHealth();
    owner_->uiState().syncExperience();
    owner_->sendPostEffects();
    owner_->chunkStreamer().resetForRespawn(session->x, session->z);
    owner_->sendInventoryAll();
}

void PlayerLifecycle::setGamemode(Gamemode mode, std::string_view cause) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    if (hooks_.gamemodeAllowed && !hooks_.gamemodeAllowed(*session, mode, session->gamemode, cause)) {
        return;
    }
    session->extras.previousGamemode = static_cast<std::int32_t>(session->gamemode);
    session->gamemode = mode;
    session->extras.allowFlight.reset();
    // parity: `ServerPlayerGameMode.changeGameModeForPlayer` lets go of any impulse.
    player::impulse::reset(*session);
    if (!session->mayFly()) {
        session->flying = false;
    }
    if (mode == Gamemode::Spectator) {
        // parity: `ServerPlayer.setGameMode` to spectator puts the shoulder parrots back.
        session->shoulderDropPending = true;
        session->flying = true;
    }
    owner_->sendCompressed(wire::protocol::play::GameEventPacket::kId, [mode](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::GameEventPacket{.event = wire::protocol::play::GameEventPacket::kChangeGameMode,
                                              .param = static_cast<float>(static_cast<std::int32_t>(mode))}
            .write(buffer);
    });
    owner_->uiState().sendAbilities();
    // Broadcast to everyone including this player: the client reads its own tab-list entry to decide it is a spectator.
    player::PlayerRegistry* registry = owner_->options().playerRegistry;
    if (registry == nullptr) {
        return;
    }
    for (PlayerSession* player : registry->snapshot()) {
        if (!player->connection().sendPlayPacket) {
            continue;
        }
        player->connection().sendPlayPacket(
            wire::protocol::play::PlayerInfoUpdatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
                wire::protocol::play::PlayerInfoUpdatePacket::gameMode(session->uuid(), mode).write(buffer);
            });
    }
}

bool PlayerLifecycle::switchWorld(world::LoadedWorld& target, double x, double y, double z) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return false;
    }
    world::LoadedWorld* previous = owner_->currentWorld();
    const std::optional<std::string> from = previous == nullptr ? std::nullopt : std::optional{previous->name()};
    if ((!from.has_value() || *from != target.name()) && hooks_.worldChanged) {
        hooks_.worldChanged(*session, target, from);
    }
    if (previous == nullptr || previous->name() == target.name()) {
        return false;
    }

    const std::array<double, 3> left{session->x, session->y, session->z};
    owner_->currentWorld_ = &target;
    session->worldName = target.name();
    session->x = x;
    session->y = y;
    session->z = z;

    owner_->entityTracker().forgetEverythingDrawn();
    owner_->sendCompressed(wire::protocol::play::RespawnPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::RespawnPacket{.spawnInfo = owner_->spawnInfo(session->gamemode),
                                            .dataToKeep = wire::protocol::play::RespawnPacket::kKeepAllData}
            .write(buffer);
    });
    owner_->sendPlayerPosition(x, y, z, session->yaw, session->pitch);
    owner_->uiState().sendAbilities();
    owner_->uiState().syncHealth();
    owner_->uiState().syncExperience();
    owner_->playerUi().worldChanged();

    owner_->chunkStreamer().resetForWorldSwitch(x, z, target.settings().viewDistance);
    dimensionTriggers(*session, *previous, target, left);

    owner_->logger().info([&] { return session->name() + " moved to world '" + target.name() + "'."; });
    return true;
}

void PlayerLifecycle::dimensionTriggers(PlayerSession& session, const world::LoadedWorld& previous,
                                        const world::LoadedWorld& target, const std::array<double, 3>& left) {
    const config::WorldDimension from = previous.declaration().dimension;
    const config::WorldDimension to = target.declaration().dimension;
    if (from == to) {
        return;
    }
    // parity: `ServerPlayer.teleport` remembers where the overworld was left for the nether.
    if (from == config::WorldDimension::Overworld && to == config::WorldDimension::Nether) {
        enteredNether_ = left;
    }
    world::AdvancementTriggers* triggers = owner_->options().advancementTriggers;
    if (triggers != nullptr) {
        // parity: `triggerDimensionChangeTriggers`, which names the dimension types.
        const auto named = [](config::WorldDimension dimension) {
            return wire::common::Identifier::vanilla(std::string{config::dimensionFacts(dimension).registryPath});
        };
        triggers->onChangedDimension(session.name(), session.uuid().toString(), named(from), named(to));
        if (from == config::WorldDimension::Nether && to == config::WorldDimension::Overworld &&
            enteredNether_.has_value()) {
            triggers->fire("nether_travel", session.name(), session.uuid().toString(),
                           world::TriggerEvent{.startPosition = enteredNether_});
        }
    }
    if (to != config::WorldDimension::Nether) {
        enteredNether_.reset();
    }
}

void PlayerLifecycle::teleportTo(double x, double y, double z, float yaw, float pitch, std::string_view cause) {
    PlayerSession* session = owner_->playerSession();
    if (hooks_.teleportAllowed && !hooks_.teleportAllowed(session, x, y, z, cause)) {
        return;
    }
    owner_->sendPlayerPosition(x, y, z, yaw, pitch);
    // parity: `Entity.teleportTo` clears the fall, so arriving is never a landing.
    owner_->combat().resetFall(*session);
    // Straight to updatePosition: chunks around the destination must start generating now, not a round trip later.
    owner_->updatePosition(x, y, z, yaw, pitch, false);
}

void PlayerLifecycle::leavePlayIfPresent() {
    PlayerSession* session = owner_->playerSession();
    player::PlayerRegistry* registry = owner_->options().playerRegistry;
    if (session == nullptr || registry == nullptr) {
        return;
    }
    // Leaving while asleep would leave the bed occupied forever.
    owner_->wakeUp();
    // Leaving with a chest open would leave its lid up.
    owner_->menus().closeOpenMenu(*session, "disconnect");
    // parity: `PlayerList.remove` awards it before the player is saved.
    player::statAwards::award(*session, "leave_game");
    // Before leaving the registry, so a save elsewhere cannot write a half-gone player.
    savePlayerState();
    registry->remove(session->uuid());
    if (owner_->options().progress != nullptr) {
        owner_->options().progress->unload(session->uuid().toString());
    }
    if (owner_->options().advancements != nullptr) {
        owner_->options().advancements->leave(session->uuid().toString());
    }
    owner_->logger().info([&] { return session->name() + " left the game"; });
    Announcement farewell;
    if (hooks_.quit) {
        farewell = hooks_.quit(
            *session, owner_->currentWorld() == nullptr ? std::nullopt : std::optional{owner_->currentWorld()->name()});
    }
    // parity: `ServerGamePacketListenerImpl.onDisconnect`.
    if (!farewell.silent) {
        owner_->broadcastToEveryone(farewell.text.empty() ? Component::translatable("multiplayer.player.left",
                                                                                    {Component::text(session->name())})
                                                                .color(wire::text::kYellow)
                                                          : wire::text::miniMessage::parse(farewell.text));
    }
    for (PlayerSession* other : registry->snapshot()) {
        if (!other->connection().sendPlayPacket) {
            continue;
        }
        other->connection().sendPlayPacket(
            wire::protocol::play::PlayerInfoRemovePacket::kId, [&](wire::io::ByteBuffer& buffer) {
                wire::protocol::play::PlayerInfoRemovePacket{.players = {session->uuid()}}.write(buffer);
            });
    }
    // Retired rather than dropped: the tick loop may be mid-way through a list that still holds this player.
    registry->retire(owner_->takePlayerSession());
}

void PlayerLifecycle::savePlayerState() {
    PlayerSession* session = owner_->playerSession();
    storage::PlayerStore* store = owner_->options().playerStore;
    if (session == nullptr || store == nullptr) {
        return;
    }
    if (!store->isOpen()) {
        // Shutdown already saved everyone; this thread is just catching up.
        owner_->logger().debug([&] { return "Skipping " + session->name() + "'s save: the database is closed."; });
        return;
    }
    store->players().save(player::playerPersistence::toStored(
        *session, owner_->worldName(), owner_->firstSeenEpochSeconds, owner_->playTimeTicks, nowEpochSeconds()));
    const std::optional<std::string> address = owner_->rememberAddress();
    if (!owner_->addressRecorded && address.has_value()) {
        store->players().setLastIp(session->uuid().toString(), *address);
        owner_->addressRecorded = true;
    }
}

std::optional<storage::StoredPlayer> PlayerLifecycle::storedSnapshot() const {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return std::nullopt;
    }
    return player::playerPersistence::toStored(*session, owner_->worldName(), owner_->firstSeenEpochSeconds,
                                               owner_->playTimeTicks, nowEpochSeconds());
}

} // namespace basalt::server::runtime
