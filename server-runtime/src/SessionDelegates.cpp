#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ChunkStreamer.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/ItemUseTimers.hpp>
#include <basalt/server/runtime/PlayerLifecycle.hpp>
#include <basalt/server/runtime/UiStateSync.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/FoodData.hpp>
#include <basalt/server/runtime/player/PlayerArrowPickup.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerItemPickup.hpp>
#include <basalt/server/runtime/player/PlayerMovementStats.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/MiniMessage.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <cmath>
#include <cstdint>

namespace basalt::server::runtime {
namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;

using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;

void sendTo(const std::vector<PlayerSession*>& players, std::int32_t packetId,
            const std::function<void(wire::io::ByteBuffer&)>& write) {
    for (const PlayerSession* player : players) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(packetId, write);
        }
    }
}

/// parity: `ServerPlayer.checkMovementStatistics` and `checkRidingStatistics`. The facts come off the
/// session because this build has no poses: sneaking stands in for crouching and sprinting in water
/// for swimming, which is what `updateSwimming` asks anyway.
void countMovement(player::PlayerSession& session, double dx, double dy, double dz, bool onGround,
                   const std::optional<std::int32_t>& feet, const std::optional<std::int32_t>& atEye) {
    const bool riding = session.vehicleId.has_value();
    const bool inWater = feet.has_value() && world::fluids::of(*feet) == world::Submersion::Water;
    const player::playerMovement::Facts facts{
        .riding = riding,
        .swimming = session.sprinting && inWater && !riding,
        .eyeInWater = atEye.has_value() && world::fluids::of(*atEye) == world::Submersion::Water,
        .inWater = inWater,
        .climbing = feet.has_value() && player::playerMovement::isClimbable(*feet),
        .onGround = onGround,
        .sprinting = session.sprinting,
        .crouching = session.sneaking};
    const std::optional<player::playerMovement::Counted> counted =
        player::playerMovement::movementStat(dx, dy, dz, facts);
    if (counted.has_value()) {
        session.stats.increment(counted->key, counted->centimetres);
    }
}

} // namespace

world::WorldState& ClientSession::world() const { return currentWorld_->state(); }

world::ChunkGenerationQueue& ClientSession::generationQueue() const { return currentWorld_->generationQueue(); }

std::string ClientSession::worldName() const { return currentWorld_ == nullptr ? "" : currentWorld_->name(); }

std::optional<std::string> ClientSession::rememberAddress() const {
    return options_.rememberIp ? remoteAddress() : std::nullopt;
}

const wire::text::LangTable& ClientSession::consoleLang() const {
    // Empty (so keys print as keys) when no language is loaded.
    static const wire::text::LangTable kEmpty;
    return options_.commands == nullptr ? kEmpty : options_.commands->environment().lang();
}

std::vector<PlayerSession*> ClientSession::playersInSameWorld() const {
    // Everyone who can actually see this player: the tab list is server-wide, entities are not.
    if (playerSession_ == nullptr || options_.playerRegistry == nullptr) {
        return {};
    }
    const std::string& name = playerSession_->worldName;
    std::vector<PlayerSession*> here;
    for (PlayerSession* player : options_.playerRegistry->snapshot()) {
        if (player->worldName == name) {
            here.push_back(player);
        }
    }
    return here;
}

play::SpawnInfo ClientSession::spawnInfo(play::Gamemode gamemode) const {
    return play::SpawnInfo{
        .dimension =
            currentWorld_ == nullptr ? 0 : config::dimensionFacts(currentWorld_->declaration().dimension).registryIndex,
        .dimensionName = currentWorld_ == nullptr ? "minecraft:overworld" : currentWorld_->levelKey(),
        .hashedSeed = 0,
        .gamemode = gamemode,
        .previousGamemode = -1,
        .isDebug = false,
        .isFlat = currentWorld_ != nullptr && currentWorld_->declaration().type == config::WorldType::Flat,
        .death = std::nullopt,
        .portalCooldown = 0,
        .seaLevel =
            currentWorld_ == nullptr ? 63 : config::dimensionFacts(currentWorld_->declaration().dimension).seaLevel};
}

void ClientSession::resendChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    chunkStreamer_->resendChunk(chunkX, chunkZ);
}

void ClientSession::sendBiomes(const std::vector<std::pair<std::int32_t, std::int32_t>>& chunks) {
    chunkStreamer_->sendBiomes(chunks);
}

void ClientSession::dismount(PlayerSession& session) { combat_->dismount(session); }

void ClientSession::sendInventory() { menus_->sendInventory(); }

void ClientSession::sendInventoryAll() { menus_->sendInventoryAll(); }

void ClientSession::sendInventorySlot(std::int32_t slot) { menus_->sendInventorySlot(slot); }

void ClientSession::wakeUp() { blockInteraction_->wakeUp(); }

void ClientSession::spawnDroppedItem(const Identifier& item, std::int32_t count, double x, double y, double z,
                                     double velocityX, double velocityY, double velocityZ) {
    blockInteraction_->spawnDroppedItem(item, count, x, y, z, velocityX, velocityY, velocityZ);
}

void ClientSession::hurt(float amount, std::string_view deathMessageId, const Identifier& damageType,
                         const std::optional<entity::DamageSource>& source) {
    combat_->hurt(amount, deathMessageId, damageType, source);
}

void ClientSession::heal(float amount) { combat_->heal(amount); }

void ClientSession::killPlayer(std::string_view deathMessageId) { combat_->killPlayer(deathMessageId); }

void ClientSession::setGamemode(play::Gamemode mode) { lifecycle_->setGamemode(mode); }

bool ClientSession::switchWorld(world::LoadedWorld& target, double x, double y, double z) {
    return lifecycle_->switchWorld(target, x, y, z);
}

void ClientSession::teleportTo(double x, double y, double z, float yaw, float pitch, std::string_view cause) {
    lifecycle_->teleportTo(x, y, z, yaw, pitch, cause);
}

std::optional<storage::StoredPlayer> ClientSession::storedSnapshot() const { return lifecycle_->storedSnapshot(); }

void ClientSession::addEffect(const Identifier& effect, std::int32_t durationTicks, std::int32_t amplifier) {
    uiState_->addEffect(effect, durationTicks, amplifier);
}

void ClientSession::syncSubmersion() { uiState_->syncSubmersion(); }

void ClientSession::syncHealth() { uiState_->syncHealth(); }

void ClientSession::syncHeldAttributes() { uiState_->syncHeldAttributes(); }

void ClientSession::syncExperience() { uiState_->syncExperience(); }

void ClientSession::sendStats() { uiState_->sendStats(); }

void ClientSession::sendTime(world::LoadedWorld& world) { uiState_->sendTime(world); }

void ClientSession::sendWeather(const world::WorldWeather& weather) { uiState_->sendWeather(weather); }

void ClientSession::sendCommandTree() { uiState_->sendCommandTree(); }

void ClientSession::applyConsumeEffects(PlayerSession& session, const std::vector<reg::ConsumeEffect>& effects) {
    uiState_->applyConsumeEffects(session, effects,
                                  [this] { return options_.nextFloat ? options_.nextFloat() : 0.5F; });
}

bool ClientSession::withinAttackReach(const PlayerSession& session, const entity::ServerEntity& target) const {
    return player::combat::withinAttackReach(session.x, session.y, session.z, target.x, target.y, target.z,
                                             target.width(), target.height());
}

void ClientSession::broadcastSound(std::string_view sound, double x, double y, double z, play::SoundSource source,
                                   float volume, float pitch) {
    const std::int64_t seed = options_.nextSeed ? options_.nextSeed() : 0;
    for (const player::PlayerSession* player : playersInSameWorld()) {
        player::playSoundFor(*player, sound, source, x, y, z, volume, pitch, seed);
    }
}

void ClientSession::sendPluginText(const wire::text::Component& text, bool actionBar) {
    sendPlayPacket(play::SystemChatPacket::kId, [&text, actionBar](wire::io::ByteBuffer& buffer) {
        play::SystemChatPacket{.content = text, .isActionBar = actionBar}.write(buffer);
    });
}

void ClientSession::sendPluginNotice(const std::string& message) {
    sendPluginText(wire::text::miniMessage::parse(message), false);
}

void ClientSession::sendActionBar(const wire::text::Component& message) { sendPluginText(message, true); }

void ClientSession::broadcastToEveryone(const wire::text::Component& message) {
    wire::text::Component text = message;
    if (options_.plugins.broadcastAllowed) {
        const std::string said = wire::text::resolvePlainText(text, consoleLang());
        std::string plain = said;
        if (!options_.plugins.broadcastAllowed(plain)) {
            return;
        }
        // Only what a plugin actually rewrote is read back as MiniMessage. Flattening every
        // broadcast through plain text costs the original its colour and its translation, so an
        // untouched join announcement arrived white and already translated, in the console's
        // language rather than each client's.
        if (plain != said) {
            text = wire::text::miniMessage::parse(plain);
        }
    }
    // parity: vanilla ends `PlayerList.broadcastSystemMessage` at `MinecraftServer.sendSystemMessage`,
    // which writes the same line to the console. Joining and leaving are the only two that come
    // through here, and they are the two an operator watches the console for.
    wire::common::Logger{"Server"}.info([&text, this] { return wire::text::resolvePlainText(text, consoleLang()); });
    if (options_.playerRegistry == nullptr) {
        return;
    }
    // Same reason as in broadcastChat: what reaches everyone has to reach both editions, and each
    // connection knows how its own client is told something.
    for (const PlayerSession* player : options_.playerRegistry->snapshot()) {
        if (player->connection().sendSystemMessage) {
            player->connection().sendSystemMessage(text);
            continue;
        }
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(play::SystemChatPacket::kId, [&text](wire::io::ByteBuffer& buffer) {
                play::SystemChatPacket{.content = text, .isActionBar = false}.write(buffer);
            });
        }
    }
}

void ClientSession::swapWithOffhand() {
    PlayerSession* session = playerSession_.get();
    if (session == nullptr || session->gamemode == play::Gamemode::Spectator) {
        return;
    }
    itemUseTimers_->stopEating();
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session->inventory.heldSlot();
    const std::optional<storage::StoredItemStack> held = session->inventory.at(heldSlot);
    if (options_.plugins.swapHandsAllowed && !options_.plugins.swapHandsAllowed(*session)) {
        sendInventorySlot(heldSlot);
        sendInventorySlot(PlayerInventory::kOffhandSlot);
        return;
    }
    session->inventory.set(heldSlot, session->inventory.at(PlayerInventory::kOffhandSlot));
    session->inventory.set(PlayerInventory::kOffhandSlot, held);
    sendInventorySlot(heldSlot);
    sendInventorySlot(PlayerInventory::kOffhandSlot);
    uiState_->syncHeldAttributes();
}

void ClientSession::tryPickupNearbyItems() {
    PlayerSession* session = playerSession_.get();
    if (session == nullptr) {
        return;
    }
    // parity: an arrow stuck in the ground is taken the same way an item is, and the same animation
    // says so; what comes back is one arrow rather than a stack.
    for (const player::PickedArrow& arrow : player::pickupNearbyArrows(*session, world())) {
        sendInventoryAll();
        const play::TakeItemEntityPacket collected{
            .collectedEntityId = arrow.entityId, .collectorEntityId = session->entityId(), .pickupItemCount = 1};
        sendTo(playersInSameWorld(), play::TakeItemEntityPacket::kId,
               [&collected](wire::io::ByteBuffer& buffer) { collected.write(buffer); });
    }
    const std::vector<player::PickedItem> taken =
        player::pickupNearbyItems(*session, world(), options_.plugins.pickupAllowed);
    if (taken.empty()) {
        return;
    }
    sendInventoryAll();
    const std::vector<PlayerSession*> watchers = playersInSameWorld();
    for (const player::PickedItem& row : taken) {
        if (row.leftover > 0) {
            // Only some of it fit, and what is left is the same entity with fewer items in it.
            const reg::Item* item = reg::items::registry().byName(row.item.item);
            if (item != nullptr) {
                const play::SetEntityDataPacket packet = play::SetEntityDataPacket::item(
                    row.item.entityId, play::WireItemStack{.itemId = item->protocolId, .count = row.leftover});
                sendTo(watchers, play::SetEntityDataPacket::kId,
                       [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
        }
        // The pickup animation only: the entity itself leaves every watcher's view through their own
        // item sync, since it is no longer in the world.
        const play::TakeItemEntityPacket collected{.collectedEntityId = row.item.entityId,
                                                   .collectorEntityId = session->entityId(),
                                                   .pickupItemCount = row.picked};
        sendTo(watchers, play::TakeItemEntityPacket::kId,
               [&collected](wire::io::ByteBuffer& buffer) { collected.write(buffer); });
    }
}

void ClientSession::updatePosition(double x, double y, double z, float yaw, float pitch, bool onGround) {
    PlayerSession* session = playerSession_.get();
    if (session == nullptr) {
        return;
    }
    // parity: `handleMovePlayer` takes only the rotation from somebody who is riding: where they are
    // is the vehicle's business.
    if (session->vehicleId.has_value()) {
        session->yaw = yaw;
        session->pitch = pitch;
        return;
    }
    // Only on a real move: a client sends its position every tick whether it moved or not, and
    // waking a handler for standing still is what makes this event expensive elsewhere.
    if ((x != session->x || y != session->y || z != session->z) && options_.plugins.moveAllowed &&
        !options_.plugins.moveAllowed(*session, session->x, session->y, session->z, x, y, z)) {
        // Put them back where they were, which is what the client is told rather than asked: it has
        // already drawn the step it took.
        sendCompressed(play::PlayerPositionPacket::kId, [session, yaw, pitch](wire::io::ByteBuffer& buffer) {
            play::PlayerPositionPacket{
                .teleportId = 0, .x = session->x, .y = session->y, .z = session->z, .yaw = yaw, .pitch = pitch}
                .write(buffer);
        });
        return;
    }
    const std::optional<std::int32_t> feet =
        world().blockState(static_cast<std::int32_t>(std::floor(x)), static_cast<std::int32_t>(std::floor(y)),
                           static_cast<std::int32_t>(std::floor(z)));
    const bool inWater = feet.has_value() && world::fluids::of(*feet) == world::Submersion::Water;
    session->food.addExhaustion(player::FoodData::movementExhaustion(x - session->x, y - session->y, z - session->z,
                                                                     session->sprinting, onGround, inWater));
    // parity: the server reads a jump off the packets, exactly here: it left the ground moving up.
    if (session->onGround && !onGround && y > session->y) {
        session->food.addExhaustion(player::FoodData::jumpExhaustion(session->sprinting));
        session->stats.increment(storage::stats::jump());
    }
    combat_->trackFall(*session, y, onGround, feet);
    const double dx = x - session->x;
    const double dy = y - session->y;
    const double dz = z - session->z;
    if (onGround) {
        lastStepSquared = (dx * dx) + (dz * dz);
    }
    const std::optional<std::int32_t> atEye =
        world().blockState(static_cast<std::int32_t>(std::floor(x)),
                           static_cast<std::int32_t>(std::floor(y + world::fluids::eyeHeight(session->sneaking))),
                           static_cast<std::int32_t>(std::floor(z)));
    countMovement(*session, dx, dy, dz, onGround, feet, atEye);
    session->x = x;
    session->y = y;
    session->z = z;
    session->yaw = yaw;
    session->pitch = pitch;
    session->onGround = onGround;
    chunkStreamer_->streamChunksAround(x, z);
    const play::EntityTeleportPacket moved{
        .entityId = session->entityId(), .x = x, .y = y, .z = z, .yaw = yaw, .pitch = pitch, .onGround = onGround};
    // The rotation above is the body. A head turns on its own, and without this one it only ever
    // tips up and down while the face stays pointing the same way.
    const play::RotateHeadPacket head{.entityId = session->entityId(), .headYaw = yaw};
    for (const PlayerSession* other : playersInSameWorld()) {
        if (other->uuid() == session->uuid() || !other->connection().sendPlayPacket) {
            continue;
        }
        other->connection().sendPlayPacket(play::EntityTeleportPacket::kId,
                                           [&moved](wire::io::ByteBuffer& buffer) { moved.write(buffer); });
        other->connection().sendPlayPacket(play::RotateHeadPacket::kId,
                                           [&head](wire::io::ByteBuffer& buffer) { head.write(buffer); });
    }
}

} // namespace basalt::server::runtime
