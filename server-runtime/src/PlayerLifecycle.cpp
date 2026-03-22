#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ChunkStreamer.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/EntityVisibilityTracker.hpp>
#include <basalt/server/runtime/ItemUseTimers.hpp>
#include <basalt/server/runtime/PlayerLifecycle.hpp>
#include <basalt/server/runtime/UiStateSync.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerPersistence.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/Component.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
namespace {

using player::PlayerSession;
using wire::protocol::play::Gamemode;
using wire::text::Component;

/// Seconds since the epoch, which is what a saved row is stamped with.
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
        // parity: the jar asks the **new** stack for its `equippable` component, so taking a piece
        // off is `UNEQUIP` and putting one on is `EQUIP`. An item that is not wearable at all in an
        // armour slot, which only a command or creative can manage, is `UNEQUIP` there and `EQUIP`
        // here: this build has no equippable component to ask.
        world::vibrations::emit(world->state(), worn[slot].has_value() ? "equip" : "unequip", session.x, session.y,
                                session.z, world->gameTime(), {.entity = session.uuid()});
    }
}

void PlayerLifecycle::tickPlayer() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    owner_->playTimeTicks++;
    if (!session->sleepingBed.has_value()) {
        session->stats.increment(storage::stats::timeSinceRest());
    }
    checkAdvancements(*session);
    if (session->dead) {
        return;
    }
    owner_->tryPickupNearbyItems();
    checkEquipment(*session);
    owner_->itemUseTimers().tickEating(*session);
    owner_->itemUseTimers().tickDrawing(*session);
    owner_->itemUseTimers().tickBlocking(*session);
    owner_->blockInteraction().tickDigging(*session);
}

void PlayerLifecycle::checkAdvancements(PlayerSession& session) {
    world::AdvancementTriggers* triggers = owner_->options().advancementTriggers;
    // parity: `ServerPlayer.inventoryMenu.broadcastChanges`, which is where vanilla checks it too:
    // once per tick after everything has moved, not once per item, so crafting a stack does not
    // check the same advancement sixty-four times.
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
    // parity: `handleClientCommand` returns without doing anything if the player is still alive, so
    // a client that sends this spuriously cannot teleport itself to spawn.
    if (session->health() > 0.0F) {
        return;
    }
    // parity: `ServerPlayer.findRespawnPositionAndUseSpawnBlock`, minus the bed and anchor checks:
    // what `/spawnpoint` set wins, and the world spawn is the fallback.
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
    if (respawnWorld == nullptr) {
        return;
    }
    const double spawnX = stored.has_value() ? stored->x : respawnWorld->spawn()[0];
    const double spawnY = stored.has_value() ? stored->y : respawnWorld->spawn()[1];
    const double spawnZ = stored.has_value() ? stored->z : respawnWorld->spawn()[2];
    session->resetForRespawn();
    session->x = spawnX;
    session->y = spawnY;
    session->z = spawnZ;
    if (hooks_.respawned) {
        hooks_.respawned(*session, *respawnWorld, spawnX, spawnY, spawnZ);
    }

    owner_->entityTracker().forgetEverythingDrawn();
    owner_->sendCompressed(wire::protocol::play::RespawnPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::RespawnPacket{.spawnInfo = owner_->spawnInfo(session->gamemode), .dataToKeep = 0}.write(
            buffer);
    });
    owner_->sendPlayerPosition(session->x, session->y, session->z, session->yaw, session->pitch);
    // parity: `PlayerList.sendLevelInfo` again, which is what a respawn goes through too.
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
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

    owner_->chunkStreamer().resetForRespawn(spawnX, spawnZ);

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
}

void PlayerLifecycle::setGamemode(Gamemode mode) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    if (hooks_.gamemodeAllowed && !hooks_.gamemodeAllowed(*session, mode, session->gamemode)) {
        return;
    }
    session->gamemode = mode;
    if (!session->mayFly()) {
        session->flying = false;
    }
    if (mode == Gamemode::Spectator) {
        session->flying = true;
    }
    owner_->sendCompressed(wire::protocol::play::GameEventPacket::kId, [mode](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::GameEventPacket{.event = wire::protocol::play::GameEventPacket::kChangeGameMode,
                                              .param = static_cast<float>(static_cast<std::int32_t>(mode))}
            .write(buffer);
    });
    owner_->uiState().sendAbilities();
    // Broadcast to everyone, this player included, because the game event above only moves the HUD:
    // a client reads its *own* tab-list entry to decide whether it is a spectator, and that is what
    // turns off collision and hides it from other players.
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
    // parity: `ServerPlayer.changeDimension` ends in `CHANGED_DIMENSION.trigger`, which names the
    // dimension types rather than the worlds.
    if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers;
        triggers != nullptr && previous != nullptr &&
        previous->declaration().dimension != target.declaration().dimension) {
        const auto named = [](config::WorldDimension dimension) {
            return wire::common::Identifier::vanilla(std::string{config::dimensionFacts(dimension).registryPath});
        };
        triggers->onChangedDimension(session->name(), session->uuid().toString(),
                                     named(previous->declaration().dimension), named(target.declaration().dimension));
    }
    if (previous == nullptr || previous->name() == target.name()) {
        return false;
    }

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

    owner_->chunkStreamer().resetForWorldSwitch(x, z, target.settings().viewDistance);

    owner_->logger().info([&] { return session->name() + " moved to world '" + target.name() + "'."; });
    return true;
}

void PlayerLifecycle::teleportTo(double x, double y, double z, float yaw, float pitch, std::string_view cause) {
    PlayerSession* session = owner_->playerSession();
    if (hooks_.teleportAllowed && !hooks_.teleportAllowed(session, x, y, z, cause)) {
        return;
    }
    owner_->sendPlayerPosition(x, y, z, yaw, pitch);
    // parity: `Entity.teleportTo` clears the fall, so arriving is never a landing.
    owner_->combat().fallDistance = 0.0;
    // Straight to updatePosition rather than waiting for the client to confirm: the world has to
    // agree the player is there now, so chunks around the destination start generating immediately
    // instead of one round trip later.
    owner_->updatePosition(x, y, z, yaw, pitch, false);
}

void PlayerLifecycle::leavePlayIfPresent() {
    PlayerSession* session = owner_->playerSession();
    player::PlayerRegistry* registry = owner_->options().playerRegistry;
    if (session == nullptr || registry == nullptr) {
        return;
    }
    // Leaving while asleep would otherwise leave the bed occupied for ever.
    owner_->wakeUp();
    // And leaving with a chest open would leave its lid up and a trapped chest powered.
    owner_->menus().closeOpenMenu(*session);
    // Before removing them from the registry, so a save triggered elsewhere cannot race this one
    // into writing a player who is already half gone.
    savePlayerState();
    registry->remove(session->uuid());
    if (owner_->options().progress != nullptr) {
        owner_->options().progress->unload(session->uuid().toString());
    }
    owner_->logger().info([&] { return session->name() + " left the game"; });
    if (hooks_.quit) {
        hooks_.quit(*session,
                    owner_->currentWorld() == nullptr ? std::nullopt : std::optional{owner_->currentWorld()->name()});
    }
    // parity: `ServerGamePacketListenerImpl.onDisconnect`, the other half of the join message.
    owner_->broadcastToEveryone(Component::translatable("multiplayer.player.left", {Component::text(session->name())})
                                    .color(wire::text::kYellow));
    for (PlayerSession* other : registry->snapshot()) {
        if (!other->connection().sendPlayPacket) {
            continue;
        }
        other->connection().sendPlayPacket(
            wire::protocol::play::PlayerInfoRemovePacket::kId, [&](wire::io::ByteBuffer& buffer) {
                wire::protocol::play::PlayerInfoRemovePacket{.players = {session->uuid()}}.write(buffer);
            });
    }
    // Handed over rather than left to die with this connection: the tick loop may be halfway
    // through a list of players that still has this one in it, on its own thread.
    registry->retire(owner_->takePlayerSession());
}

void PlayerLifecycle::savePlayerState() {
    PlayerSession* session = owner_->playerSession();
    storage::PlayerStore* store = owner_->options().playerStore;
    if (session == nullptr || store == nullptr) {
        return;
    }
    if (!store->isOpen()) {
        // The server is shutting down and already saved everyone before disconnecting them. This
        // connection's own thread is just catching up.
        owner_->logger().debug([&] { return "Skipping " + session->name() + "'s save: the database is closed."; });
        return;
    }
    store->players().save(player::playerPersistence::toStored(
        *session, owner_->worldName(), owner_->firstSeenEpochSeconds, owner_->playTimeTicks, nowEpochSeconds()));
    // After the save, because there has to be a row to update.
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
