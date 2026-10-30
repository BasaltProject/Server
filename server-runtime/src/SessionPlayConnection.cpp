#include "SessionPlayShared.hpp"

#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ChunkStreamer.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/FireworkSystem.hpp>
#include <basalt/server/runtime/ItemUseTimers.hpp>
#include <basalt/server/runtime/PlayerLifecycle.hpp>
#include <basalt/server/runtime/UiStateSync.hpp>
#include <basalt/server/runtime/VillagerTradeController.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/command/commands/BossBarCommand.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerBooks.hpp>
#include <basalt/server/runtime/player/PlayerGliding.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerPersistence.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSleep.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/plugins/PlayerHookEvents.hpp>
#include <basalt/server/runtime/plugins/PluginItemConversion.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/Dialogs.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/PlayerAdvancements.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealDialogs.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/RecipeBook.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/nbt/Snbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/protocol/play/VehiclePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/MiniMessage.hpp>
#include <basalt/wire/text/Serializers.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <expected>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime {
using namespace sessionPlay;

player::PlayerConnection ClientSession::playerConnection() {
    player::PlayerConnection connection;
    connection.protocolVersion = handshake_.has_value() ? handshake_->protocolVersion : 0;
    connection.connectedHost = handshake_.has_value() ? handshake_->hostName : "";
    connection.connectedPort = handshake_.has_value() ? handshake_->port : 0;
    connection.transferred =
        handshake_.has_value() && handshake_->intent == wire::protocol::handshake::ClientIntent::Transfer;
    connection.address = remoteAddress();
    world::RealEnchantments* enchantments = options_.enchantments;
    connection.enchantmentIds = [enchantments](const Identifier& name) {
        return enchantments == nullptr ? std::nullopt : enchantments->protocolIdOf(name);
    };
    connection.sendSystemMessage = [this](const wire::text::Component& message) { sendPluginText(message, false); };
    connection.sendChatMessage = [this](const chat::PlayerChatMessage& message, const chat::BoundChatType& chatType) {
        deliverChat(message, chatType);
    };
    connection.sendPlayPacket = [this](std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& write) {
        sendPlayPacket(id, write);
    };
    connection.packetEndpoint = [this] { return endpoint(); };
    connection.broadcastEffects = [this](std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                                         const std::vector<world::BlockEffect>& effects) {
        blockInteraction_->broadcastEffects(play::BlockPos{x, y, z}, stateId, effects);
    };
    connection.sendBlockEvent = [this](const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                       const std::int32_t stateId, const std::int32_t b0, const std::int32_t b1) {
        const play::BlockEventPacket packet{.pos = play::BlockPos{x, y, z},
                                            .b0 = static_cast<std::uint8_t>(b0),
                                            .b1 = static_cast<std::uint8_t>(b1),
                                            .blockId = reg::blockSupport::blockRegistryIdOf(stateId)};
        sendPlayPacket(play::BlockEventPacket::kId, [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    };
    connection.sendLevelEvent = [this](const std::int32_t type, const std::int32_t x, const std::int32_t y,
                                       const std::int32_t z, const std::int32_t data) {
        const play::LevelEventPacket packet{.event = type, .pos = play::BlockPos{x, y, z}, .data = data};
        sendPlayPacket(play::LevelEventPacket::kId, [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    };
    connection.resendChunk = [this](std::int32_t chunkX, std::int32_t chunkZ) { resendChunk(chunkX, chunkZ); };
    connection.handOverToRegion = [this](world::LoadedWorld& world) { handOverToRegionOf(world); };
    connection.sendBiomes = [this](const std::vector<std::pair<std::int32_t, std::int32_t>>& columns) {
        sendBiomes(columns);
    };
    connection.sendWeather = [this](const world::WorldWeather& weather) { sendWeather(weather); };
    connection.disconnect = [this](const wire::text::Component& reason) { disconnect(reason); };
    connection.setGamemode = [this](play::Gamemode mode, std::string_view cause) { setGamemode(mode, cause); };
    connection.sendInventoryAll = [this] { sendInventoryAll(); };
    connection.setHeldSlot = [this](const std::int32_t slot) {
        sendCompressed(play::SetHeldSlotPacket::kId,
                       [slot](wire::io::ByteBuffer& buffer) { play::SetHeldSlotPacket{.slot = slot}.write(buffer); });
    };
    connection.heal = [this](float amount, std::string_view cause) { heal(amount, cause); };
    connection.hurt = [this](float amount, std::string_view deathMessageId, const Identifier& damageType,
                             const std::optional<entity::DamageSource>& source) {
        hurt(amount, deathMessageId, damageType, source);
    };
    connection.addEffect = [this](const Identifier& effect, std::int32_t durationTicks, std::int32_t amplifier) {
        addEffect(effect, durationTicks, amplifier);
    };
    connection.effectAllowed = [this](const Identifier& effect, std::int32_t durationTicks, std::int32_t amplifier) {
        return uiState_->effectAllowed(effect, durationTicks, amplifier);
    };
    connection.sendPlayerTeam = [this](const std::string& team, const std::uint8_t method,
                                       const std::optional<play::SetPlayerTeamPacket::Parameters>& parameters,
                                       const std::vector<std::string>& members) {
        sendPlayPacket(play::SetPlayerTeamPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::SetPlayerTeamPacket{.teamName = team, .method = method, .parameters = parameters, .members = members}
                .write(buffer);
        });
    };
    connection.sendObjective = [this](const std::string& objective, const std::uint8_t method,
                                      const std::optional<wire::text::Component>& title) {
        sendPlayPacket(play::SetObjectivePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::SetObjectivePacket{.objectiveName = objective, .method = method, .displayName = title}.write(buffer);
        });
    };
    connection.sendDisplayObjective = [this](const play::DisplaySlot slot, const std::string& objective) {
        sendPlayPacket(play::SetDisplayObjectivePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::SetDisplayObjectivePacket{.slot = slot, .objectiveName = objective}.write(buffer);
        });
    };
    connection.sendScore = [this](const std::string& holder, const std::string& objective, const std::int32_t value) {
        sendPlayPacket(play::SetScorePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::SetScorePacket{.owner = holder, .objectiveName = objective, .score = value}.write(buffer);
        });
    };
    connection.sendScoreRemoved = [this](const std::string& holder, const std::optional<std::string>& objective) {
        sendPlayPacket(play::ResetScorePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::ResetScorePacket{.owner = holder, .objectiveName = objective}.write(buffer);
        });
    };
    connection.sendSpawnPosition = [this](const world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
                                          const std::int32_t z, const float angle) {
        sendPlayPacket(play::SpawnPositionPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::SpawnPositionPacket{
                .globalPos = play::GlobalPos{.dimensionName = world.levelKey(), .location = play::BlockPos{x, y, z}},
                .yaw = angle,
                .pitch = 0.0F}
                .write(buffer);
        });
    };
    connection.stopSound = [this](const std::optional<std::string>& sound,
                                  const std::optional<play::SoundSource>& source) {
        sendPlayPacket(play::StopSoundPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::StopSoundPacket{.sound = sound, .source = source}.write(buffer);
        });
    };
    connection.startItemCooldown = [this](const Identifier& item, const std::int32_t durationTicks) {
        sendPlayPacket(play::CooldownPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::CooldownPacket{.cooldownGroup = item.toString(), .durationTicks = durationTicks}.write(buffer);
        });
    };
    connection.sendBlockDestruction = [this](const std::int32_t entityId, const std::int32_t x, const std::int32_t y,
                                             const std::int32_t z, const std::int32_t stage, float) {
        sendPlayPacket(play::BlockDestructionPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::BlockDestructionPacket{
                .entityId = entityId, .pos = play::BlockPos{x, y, z}, .stage = static_cast<std::uint8_t>(stage)}
                .write(buffer);
        });
    };
    connection.sendTitlePiece = [this](const player::TitlePiece piece, const wire::text::Component& text,
                                       const std::int32_t fadeIn, const std::int32_t stay, const std::int32_t fadeOut) {
        switch (piece) {
        case player::TitlePiece::Times:
            sendPlayPacket(play::SetTitlesAnimationPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                play::SetTitlesAnimationPacket{.fadeIn = fadeIn, .stay = stay, .fadeOut = fadeOut}.write(buffer);
            });
            return;
        case player::TitlePiece::Clear:
        case player::TitlePiece::Reset:
            sendPlayPacket(play::ClearTitlesPacket::kId, [piece](wire::io::ByteBuffer& buffer) {
                play::ClearTitlesPacket{.resetTimes = piece == player::TitlePiece::Reset}.write(buffer);
            });
            return;
        case player::TitlePiece::Title:
        case player::TitlePiece::Subtitle:
        case player::TitlePiece::ActionBar:
            break;
        }
        const std::int32_t id = piece == player::TitlePiece::Title      ? play::kSetTitleTextId
                                : piece == player::TitlePiece::Subtitle ? play::kSetSubtitleTextId
                                                                        : play::kSetActionBarTextId;
        sendPlayPacket(id,
                       [&text](wire::io::ByteBuffer& buffer) { play::TitleTextPacket{.text = text}.write(buffer); });
    };
    connection.setCamera = [this](const std::int32_t entityId) {
        sendPlayPacket(play::SetCameraPacket::kId, [entityId](wire::io::ByteBuffer& buffer) {
            play::SetCameraPacket{.cameraEntityId = entityId}.write(buffer);
        });
    };
    connection.showDialog = [this](const std::int32_t dialogId, const world::dialogs::Dialog&) {
        sendPlayPacket(play::ShowDialogPacket::kId, [dialogId](wire::io::ByteBuffer& buffer) {
            play::ShowDialogPacket{.dialogId = dialogId}.write(buffer);
        });
    };
    connection.reenterConfiguration = [this] { return reenterConfiguration(); };
    connection.showDialogData = [this](const wire::nbt::NbtCompound& dialog) {
        play::ShowDialogPacket packet;
        packet.inlineDialog = dialog;
        sendPlayPacket(play::ShowDialogPacket::kId, [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        return true;
    };
    connection.showDialogNamed = [this](const std::string& id) {
        const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(id);
        const std::optional<std::int32_t> dialogId =
            options_.dialogs == nullptr || !parsed.has_value() ? std::nullopt : options_.dialogs->protocolIdOf(*parsed);
        if (!dialogId.has_value()) {
            return false;
        }
        sendPlayPacket(play::ShowDialogPacket::kId, [dialogId](wire::io::ByteBuffer& buffer) {
            play::ShowDialogPacket{.dialogId = *dialogId}.write(buffer);
        });
        return true;
    };
    connection.showForm = [this](const world::dialogs::Dialog& dialog) {
        std::int32_t id = 0;
        auto held = std::make_shared<const world::dialogs::Dialog>(dialog);
        {
            const std::lock_guard<std::mutex> guard{formsLock_};
            id = nextFormId_++;
            forms_[id] = held;
        }
        play::ShowDialogPacket packet;
        packet.inlineDialog = world::dialogs::toNbt(*held, "basalt:form/" + std::to_string(id));
        sendPlayPacket(play::ShowDialogPacket::kId, [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    };
    connection.sendWaypoint = [this](const wire::common::Uuid& waypoint, std::int32_t, const std::string& style,
                                     const std::optional<std::int32_t>& color, const double x, const double y,
                                     const double z, const bool tracked) {
        sendPlayPacket(play::TrackedWaypointPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::TrackedWaypointPacket{.operation = tracked ? play::TrackedWaypointPacket::Operation::Track
                                                             : play::TrackedWaypointPacket::Operation::Untrack,
                                        .entityUuid = waypoint,
                                        .style = style,
                                        .color = color,
                                        .x = static_cast<std::int32_t>(std::floor(x)),
                                        .y = static_cast<std::int32_t>(std::floor(y)),
                                        .z = static_cast<std::int32_t>(std::floor(z))}
                .write(buffer);
        });
    };
    connection.sendEffect = [this](const world::ActiveEffect& effect) { uiState_->sendEffect(effect); };
    connection.sendEffectRemoved = [this](const Identifier& effect) { uiState_->sendEffectRemoved(effect); };
    connection.switchWorld = [this](world::LoadedWorld& target, double x, double y, double z) {
        return switchWorld(target, x, y, z);
    };
    connection.teleportTo = [this](double x, double y, double z, float yaw, float pitch) {
        teleportTo(x, y, z, yaw, pitch);
    };
    connection.dismount = [this] {
        if (player::PlayerSession* rider = playerSession(); rider != nullptr) {
            dismount(*rider);
        }
    };
    connection.wakeUp = [this] { wakeUp(); };
    connection.openMerchant = [this](const wire::text::Component& title, std::vector<world::trades::Offer> offers) {
        return trades_->openCustom(title, std::move(offers));
    };
    connection.openTrades = [this](std::int32_t villagerId) {
        entity::ServerEntity* villager = currentWorld_ == nullptr ? nullptr : currentWorld_->state().entity(villagerId);
        if (villager == nullptr || villager->removed) {
            return false;
        }
        trades_->openTrades(*villager);
        return trades_->open();
    };
    connection.resendRecipes = [this] {
        PlayerSession* session = playerSession();
        if (session == nullptr) {
            return;
        }
        sendStonecutterRecipes();
        if (options_.progress != nullptr && options_.recipes != nullptr) {
            const std::string uuid = session->uuid().toString();
            const std::set<wire::common::Identifier> known = options_.progress->knownRecipes(uuid);
            world::recipeBook::resend(*session, *options_.recipes, {known.begin(), known.end()},
                                      options_.progress->highlightedRecipes(uuid));
        }
    };
    connection.attachFirework = [this](const std::string& text) -> std::int32_t {
        PlayerSession* session = playerSession();
        const std::optional<storage::StoredItemStack> stack = plugins::stackFromSnbt(text);
        if (session == nullptr || currentWorld_ == nullptr || options_.fireworks == nullptr || !stack.has_value() ||
            stack->id != "minecraft:firework_rocket") {
            return -1;
        }
        wire::nbt::NbtCompound item;
        item.putString("id", stack->id);
        item.putInt("count", 1);
        if (stack->components.size() > 0) {
            item.set("components", wire::nbt::NbtTag::ofCompound(stack->components));
        }
        const entity::ServerEntity* rocket = options_.fireworks->attach(
            *currentWorld_, std::move(item), session->entityId(), session->x, session->y, session->z);
        return rocket == nullptr ? -1 : rocket->entityId();
    };
    connection.retitleScreen = [this](const wire::text::Component& title) { return retitleScreen(title); };
    connection.refreshProfile = [this] { lifecycle_->refreshProfile(); };
    connection.sleepAt = [this](std::int32_t x, std::int32_t y, std::int32_t z, bool force) {
        PlayerSession* session = playerSession();
        if (session == nullptr || currentWorld_ == nullptr || session->sleepingBed.has_value() || session->dead) {
            return false;
        }
        const std::optional<std::int32_t> state = currentWorld_->state().blockState(x, y, z);
        if (!state.has_value() || !world::beds::isBed(*state)) {
            return false;
        }
        return !player::sleepInBedFor(blockInteraction_->sleepContext(), *session, *currentWorld_, x, y, z, *state,
                                      force)
                    .has_value();
    };
    connection.itemUse = [this](std::string_view operation, std::int64_t value) -> std::int64_t {
        PlayerSession* session = playerSession();
        const std::optional<ItemUseTimers::UseState> state = itemUseTimers_->useState();
        if (operation == "used") {
            return state.has_value() ? state->used : -1;
        }
        if (operation == "remaining") {
            return state.has_value() ? state->duration - state->used : -1;
        }
        if (session == nullptr) {
            return 0;
        }
        if (operation == "set_remaining") {
            return itemUseTimers_->setRemaining(static_cast<std::int32_t>(value)) ? 1 : 0;
        }
        if (operation == "complete") {
            itemUseTimers_->completeUse(*session);
            return state.has_value() ? 1 : 0;
        }
        if (operation == "clear") {
            itemUseTimers_->clearUse();
            return state.has_value() ? 1 : 0;
        }
        const std::int32_t slot = value != 0 ? player::PlayerInventory::kOffhandSlot
                                             : player::PlayerInventory::kHotbarStart + session->inventory.heldSlot();
        return operation == "start" && itemUseTimers_->startUse(*session, slot) ? 1 : 0;
    };
    connection.sendTime = [this](const world::LoadedWorld& world) { uiState_->sendTime(world); };
    connection.sendActionBar = [this](const wire::text::Component& message) { sendPluginText(message, true); };
    connection.sendTitle = [this](const wire::text::Component& title, const wire::text::Component& subtitle,
                                  std::int32_t fadeIn, std::int32_t stay, std::int32_t fadeOut) {
        // Timings go first: sent after the text, the client applies them to the next title.
        const play::SetTitlesAnimationPacket timings{.fadeIn = fadeIn, .stay = stay, .fadeOut = fadeOut};
        sendCompressed(play::SetTitlesAnimationPacket::kId,
                       [&timings](wire::io::ByteBuffer& buffer) { timings.write(buffer); });
        if (!wire::text::toPlainText(subtitle).empty()) {
            const play::TitleTextPacket under{.text = subtitle};
            sendCompressed(play::kSetSubtitleTextId, [&under](wire::io::ByteBuffer& buffer) { under.write(buffer); });
        }
        const play::TitleTextPacket over{.text = title};
        sendCompressed(play::kSetTitleTextId, [&over](wire::io::ByteBuffer& buffer) { over.write(buffer); });
    };
    connection.sendCommandTree = [this] { sendCommandTree(); };
    connection.syncExperience = [this] { syncExperience(); };
    connection.respawn = [this] { lifecycle_->respawn(); };
    connection.syncHealth = [this] { uiState_->syncHealth(); };
    connection.tracksEntity = [this](std::int32_t entityId) { return entityTracker_->isTracking(entityId); };
    connection.sentChunks = [this] { return chunkStreamer_->sentChunks(); };
    connection.overrideViewDistance = [this](std::optional<std::int32_t> distance) {
        chunkStreamer_->overrideViewDistance(distance);
    };
    connection.chat = [this](const std::string& message) {
        if (secureChat_ == nullptr || !tryHandleChat(message, false)) {
            return;
        }
        const std::int64_t now = chat::nowMillis();
        std::expected<chat::PlayerChatMessage, std::string> built = secureChat_->unpack(
            std::nullopt, chat::SignedBody{.content = message, .timestamp = now, .salt = 0, .lastSeen = {}}, now);
        if (built.has_value()) {
            broadcastChat(*built);
        }
    };
    connection.killPlayer = [this](std::string_view deathMessageId) { killPlayer(deathMessageId); };
    connection.sendBlockChange = [this](const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                        const std::int32_t stateId) {
        const play::ClientboundBlockUpdatePacket packet{.pos = play::BlockPos{x, y, z}, .blockStateId = stateId};
        sendCompressed(play::ClientboundBlockUpdatePacket::kId,
                       [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    };
    connection.sendCombatDeath = [this](const wire::text::Component& message) {
        if (currentWorld_ != nullptr) {
            playerSession()->lastDeath = wire::protocol::play::GlobalPos{
                .dimensionName = currentWorld_->levelKey(),
                .location = play::BlockPos{static_cast<std::int32_t>(std::floor(playerSession()->x)),
                                           static_cast<std::int32_t>(std::floor(playerSession()->y)),
                                           static_cast<std::int32_t>(std::floor(playerSession()->z))}};
        }
        sendCompressed(wire::protocol::play::PlayerCombatKillPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::PlayerCombatKillPacket{.playerId = playerSession()->entityId(), .message = message}
                .write(buffer);
        });
    };
    connection.storedSnapshot = [this] { return storedSnapshot(); };
    connection.openPluginMenu = [this](const std::string& title, std::int32_t rows,
                                       const std::map<std::int32_t, server::plugins::PluginItem>& items,
                                       std::function<void(std::int32_t)> onClick) {
        menus_->openPluginMenu(title, rows, menuItemsOf(items), std::move(onClick));
    };
    connection.refreshPluginMenu = [this](const std::map<std::int32_t, server::plugins::PluginItem>& items) {
        menus_->refreshPluginMenu(menuItemsOf(items));
    };
    connection.closeOpenMenu = [this] { menus_->closeOpenMenu(); };
    connection.window = [this]() -> server::plugins::PluginWindow* { return menus_.get(); };
    return connection;
}

void ClientSession::enterPlay() {
    if (!switchingToConfiguration_.load() && options_.gate != nullptr) {
        options_.gate->awaitJoinSlot(options_.tuning.maxJoinsPerTick);
    }
    state_ = wire::protocol::ProtocolState::Play;
    if (switchingToConfiguration_.exchange(false)) {
        entityTracker_->forgetEverythingDrawn();
        chunkStreamer_->forgetAll();
    }
    player::PlayerRegistry* registry = options_.playerRegistry;
    if (registry == nullptr || !pendingProfile_.has_value()) {
        connection_.close();
        return;
    }
    const std::int32_t entityId = registry->allocateEntityId();

    // Decided before anything is sent (the login carries a position); an undeclared stored world falls back to default.
    const std::optional<storage::StoredPlayer> stored =
        options_.playerStore == nullptr || !options_.playerStore->isOpen()
            ? std::nullopt
            : options_.playerStore->players().load(pendingProfile_->id.toString());
    const auto heldWorlds =
        options_.worldsNow ? options_.worldsNow() : std::map<std::string, std::shared_ptr<world::LoadedWorld>>{};
    auto worlds = options_.worlds;
    if (options_.worldsNow) {
        worlds.clear();
        for (const auto& [name, held] : heldWorlds)
            worlds.emplace(name, held.get());
    }
    const auto requested = stored.has_value() ? std::optional{stored->world} : std::nullopt;
    world::LoadedWorld* world = options_.defaultWorld;
    if (requested.has_value()) {
        const auto found = worlds.find(*requested);
        if (found != worlds.end()) {
            world = found->second;
        }
    }
    if (world == nullptr) {
        connection_.close();
        return;
    }
    if (requested.has_value() && *requested != world->name()) {
        logger_.info([this, &requested, world] {
            return pendingProfile_->name + "'s world '" + *requested + "' is not loaded; sending them to '" +
                   world->name() + "'.";
        });
    }
    currentWorld_ = world;

    const std::int32_t chunkRadius = world->settings().viewDistance;
    const std::array<double, 3>& spawn = world->spawn();
    // Before the login packet, because everything after it describes this player.
    auto session = std::make_unique<PlayerSession>(pendingProfile_->id, pendingProfile_->name, entityId,
                                                   playerConnection(), options_.defaultGamemode);
    session->x = spawn[0];
    session->y = spawn[1];
    session->z = spawn[2];
    session->flying = options_.defaultGamemode == play::Gamemode::Spectator;
    session->worldName = world->name();
    // Restored before any packet: the login carries game mode and position, so later means announce then correct.
    if (stored.has_value()) {
        player::playerPersistence::restore(*session, *stored);
        firstSeenEpochSeconds = stored->firstSeenEpochSeconds;
        playTimeTicks = stored->playTimeTicks;
        // Only the position is meaningless when the stored world is gone; the rest is the player's.
        if (stored->world != world->name()) {
            session->x = spawn[0];
            session->y = spawn[1];
            session->z = spawn[2];
        }
        logger_.info([&session] { return "Loaded " + session->name() + " from players.db."; });
    }
    // parity: `ServerPlayer.calculateGameModeForNewPlayer`, which a dedicated server applies to operators too.
    if (options_.tuning.forceGamemode) {
        session->gamemode = options_.defaultGamemode;
        session->flying = session->gamemode == play::Gamemode::Spectator;
    }
    session->profileProperties.clear();
    for (const wire::auth::SessionProfileProperty& property : pendingProfile_->properties) {
        session->profileProperties.push_back(wire::protocol::login::GameProfileProperty{
            .name = property.name, .value = property.value, .signature = property.signature});
    }
    session->skinParts = pendingSkinParts_;
    session->extras.client = pendingOptions_;
    session->extras.lastActionMillis = steadyMillis();
    session->extras.brand = pendingBrand_;
    session->mainArm = pendingMainHand_;
    session->chatVisibility.store(static_cast<PlayerSession::ChatVisibility>(std::clamp(pendingChatVisibility_, 0, 2)));
    playerSession_ = std::move(session);
    PlayerSession& player = *playerSession_;
    if (options_.events != nullptr) {
        plugins::playerHooks::install(player, *options_.events);
    }
    secureChat_ = std::make_unique<chat::SecureChat>(
        player.uuid(), options_.enforceSecureProfile && !translatedClient(), options_.servicesKeys);

    std::vector<std::string> worldNames;
    worldNames.reserve(worlds.size());
    for (const auto& [name, loaded] : worlds) {
        worldNames.push_back(loaded->levelKey());
    }
    const bool online = options_.onlineMode != nullptr;
    sendCompressed(play::LoginPlayPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        play::LoginPlayPacket{.entityId = entityId,
                              .isHardcore = currentWorld() != nullptr && currentWorld()->hardcore(),
                              .worldNames = worldNames,
                              .maxPlayers = options_.maxPlayers ? options_.maxPlayers() : 20,
                              .viewDistance = chunkRadius,
                              .simulationDistance = world->tunables().simulationDistance.value_or(chunkRadius),
                              .reducedDebugInfo = false,
                              .enableRespawnScreen = true,
                              .doLimitedCrafting = false,
                              .spawnInfo = spawnInfo(player.gamemode),
                              .onlineMode = online,
                              .enforcesSecureChat = options_.enforceSecureProfile && !translatedClient()}
            .write(buffer);
    });

    // Straight after login as in vanilla: abilities gate flying, and slash commands need the command tree.
    uiState_->sendAbilities();
    sendStonecutterRecipes();
    uiState_->sendCommandTree();

    // Without these the client's loading screen never dismisses: it has no centre or radius to count chunks against.
    chunkStreamer_->setRequestedViewDistance(pendingViewDistance_);
    const server::world::ChunkTracker::Delta initialChunks =
        chunkStreamer_->initialJoin(player.x, player.z, chunkRadius);

    // parity: `PlayerList.sendLevelInfo` opens with the world border.
    sendCompressed(play::InitializeBorderPacket::kId,
                   [world](wire::io::ByteBuffer& buffer) { world->border().initializePacket().write(buffer); });
    uiState_->sendTime(*world);
    if (world->weather().raining) {
        uiState_->sendWeather(world->weather());
    }
    // parity: `CustomBossEvents.onPlayerConnect`.
    if (options_.bossBars != nullptr) {
        for (const command::commands::CustomBossBar* bar : options_.bossBars->barsFor(player)) {
            sendCompressed(play::BossEventPacket::kId, [bar](wire::io::ByteBuffer& buffer) {
                play::BossEventPacket{.id = bar->uuid, .operation = bar->addOperation()}.write(buffer);
            });
        }
    }
    sendCompressed(play::SpawnPositionPacket::kId, [world, &spawn](wire::io::ByteBuffer& buffer) {
        play::SpawnPositionPacket{.globalPos =
                                      play::GlobalPos{.dimensionName = world->levelKey(),
                                                      .location = play::BlockPos{static_cast<std::int32_t>(spawn[0]),
                                                                                 static_cast<std::int32_t>(spawn[1]),
                                                                                 static_cast<std::int32_t>(spawn[2])}},
                                  .yaw = 0.0F,
                                  .pitch = 0.0F}
            .write(buffer);
    });
    sendPlayerPosition(player.x, player.y, player.z, player.yaw, player.pitch);

    const std::vector<PlayerSession*> others = registry->snapshot();
    registry->add(&player);
    // Advancements and recipe book go first: `limited_crafting` consults the book on the first click.
    if (options_.progress != nullptr) {
        options_.progress->load(player.uuid().toString());
        if (options_.recipes != nullptr) {
            // parity: `ServerRecipeBook.sendInitialRecipeBook`.
            const std::string uuid = player.uuid().toString();
            const std::set<wire::common::Identifier> known = options_.progress->knownRecipes(uuid);
            world::recipeBook::sendSettings(player, options_.progress->bookSettings(uuid));
            world::recipeBook::resend(player, *options_.recipes, {known.begin(), known.end()},
                                      options_.progress->highlightedRecipes(uuid));
        }
    }
    // The client knows nothing of the effects the player logged out with until it is told.
    {
        const float absorption = player.absorption;
        for (const auto& [id, effect] : player.effects) {
            uiState_->sendEffect(effect);
        }
        player.absorption = absorption;
    }
    if (options_.advancements != nullptr) {
        options_.advancements->join(player.uuid().toString());
    }
    if (options_.advancementTriggers != nullptr) {
        options_.advancementTriggers->grantAutomatic(player.name(), player.uuid().toString());
    }
    logger_.info([this, &player, entityId] {
        // parity: `PlayerList.placeNewPlayer` logs `Connection.getLoggableAddress`.
        const std::string address =
            options_.tuning.logIps ? "/" + peerAddress() + ":" + std::to_string(peerPort()) : "IP hidden";
        return player.name() + "[" + address + "] logged in with entity id " + std::to_string(entityId) + " at (" +
               oneDecimal(player.x) + ", " + oneDecimal(player.y) + ", " + oneDecimal(player.z) + ")";
    });
    logger_.info([&player] { return player.name() + " joined the game"; });
    auto welcome = std::make_shared<Announcement>();
    if (options_.plugins.joined) {
        // Handed to the owner: a decider only runs on the plugin thread, and from here the host drops it.
        static_cast<void>(onOwnerWait(
            [this, welcome] {
                const PlayerSession* joined = playerSession_.get();
                if (joined != nullptr) {
                    *welcome = options_.plugins.joined(*joined);
                }
            },
            std::chrono::seconds(5)));
    }

    std::vector<play::PlayerInfoEntry> entries;
    entries.reserve(others.size() + 1);
    for (const PlayerSession* other : others) {
        entries.push_back(entryFor(*other));
    }
    entries.push_back(entryFor(player));
    sendCompressed(play::PlayerInfoUpdatePacket::kId, [&entries](wire::io::ByteBuffer& buffer) {
        play::PlayerInfoUpdatePacket::initializing(entries).write(buffer);
    });

    uiState_->syncHealth();
    uiState_->syncExperience();
    // parity: `PlayerList.placeNewPlayer` sends the saved post effects after the mob effects.
    sendPostEffects();
    sendInventory();

    chunkStreamer_->sendInitialChunks(initialChunks);

    // Only players who cannot see this one as an entity; the entity sync sends the entry before the spawn for the rest.
    const play::PlayerInfoUpdatePacket joining = play::PlayerInfoUpdatePacket::initializing({entryFor(player)});
    for (const PlayerSession* other : others) {
        if (other->worldName == player.worldName || !other->connection().sendPlayPacket) {
            continue;
        }
        other->connection().sendPlayPacket(play::PlayerInfoUpdatePacket::kId,
                                           [&joining](wire::io::ByteBuffer& buffer) { joining.write(buffer); });
    }

    // Drawn here: the outbound loop sends chunks first and would leave the scoreboard behind the terrain.
    uiState_->tickUi();

    if (!welcome->silent) {
        broadcastToEveryone(welcome->text.empty()
                                ? wire::text::Component::translatable("multiplayer.player.joined",
                                                                      {wire::text::Component::text(player.name())})
                                      .color(wire::text::kYellow)
                                : wire::text::miniMessage::parse(welcome->text));
    }

    // Last: started earlier, this thread interleaves chunk batches with join packets, which the client reads as desync.
    startOutboundLoop();
}

} // namespace basalt::server::runtime
