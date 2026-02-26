#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ChunkStreamer.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/PlayerLifecycle.hpp>
#include <basalt/server/runtime/UiStateSync.hpp>
#include <basalt/server/runtime/VillagerTradeController.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/command/commands/BossBarCommand.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerPersistence.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/Serializers.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;

using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;

/// parity: `ServerboundMovePlayerPacket`'s three shapes beyond the full one, which carry a
/// rotation, a status byte, or both.
constexpr std::int32_t kMovePlayerRotId = 0x1f;
constexpr std::int32_t kMovePlayerStatusOnlyId = 0x20;
constexpr std::int32_t kSwingPacketId = 0x40;
constexpr std::int32_t kServerboundClientInformationId = 0x0d;
constexpr std::int32_t kServerboundPlayerInputId = 0x2b;

// parity: `Input`'s own bit order.
constexpr std::int32_t kForwardInputFlag = 0x01;
constexpr std::int32_t kBackInputFlag = 0x02;
constexpr std::int32_t kLeftInputFlag = 0x04;
constexpr std::int32_t kRightInputFlag = 0x08;
constexpr std::int32_t kSneakInputFlag = 0x20;

/// What the client's own answer to "how fast can you take chunks" is clamped to.
constexpr float kMinChunksPerTick = 0.01F;
constexpr float kMaxChunksPerTick = 64.0F;

/// A coordinate as the log wants it: one decimal, which is as much as anybody reads off it.
std::string oneDecimal(double value) {
    // The sign is kept apart from the digits: dividing a negative tenth by ten is zero, so anything
    // between -1 and 0 came out looking positive.
    const auto tenths = static_cast<std::int64_t>(std::llround(std::abs(value) * 10.0));
    const std::string sign = value < 0.0 && tenths != 0 ? "-" : "";
    return sign + std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

/// What a plugin drew, as what the window machinery draws: the same three fields, and the rest of a
/// `PluginItem` (damage, enchantments) is not something a menu button has.
[[nodiscard]] std::map<std::int32_t, PluginMenuItem>
menuItemsOf(const std::map<std::int32_t, server::plugins::PluginItem>& items) {
    std::map<std::int32_t, PluginMenuItem> drawn;
    for (const auto& [slot, item] : items) {
        drawn.emplace(slot, PluginMenuItem{.id = item.id, .count = item.count, .name = item.name, .lore = item.lore});
    }
    return drawn;
}

play::PlayerInfoEntry entryFor(const PlayerSession& player) {
    return play::PlayerInfoEntry{.uuid = player.uuid(),
                                 .name = player.name(),
                                 .listed = true,
                                 .gamemode = player.gamemode,
                                 .properties = player.profileProperties};
}

} // namespace

void ClientSession::sendStonecutterRecipes() {
    world::RealRecipes* recipes = options_.recipes;
    if (recipes == nullptr) {
        return;
    }
    // The recipe book's own item sets go out empty: they only drive the highlighting of what can be
    // crafted, and drawing a stonecutter's buttons does not depend on them.
    std::vector<play::StonecutterOption> options;
    for (const server::world::crafting::Recipe* recipe : recipes->stonecutting()) {
        const reg::Item* result = reg::items::registry().byName(recipe->result.item);
        if (result == nullptr) {
            continue;
        }
        std::vector<std::int32_t> inputs;
        for (const std::string& item : recipe->ingredient.items) {
            const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(item);
            const reg::Item* entry = id.has_value() ? reg::items::registry().byName(*id) : nullptr;
            if (entry != nullptr) {
                inputs.push_back(entry->protocolId);
            }
        }
        if (inputs.empty()) {
            continue;
        }
        options.push_back(play::StonecutterOption{.inputItemIds = std::move(inputs),
                                                  .resultItemId = result->protocolId,
                                                  .resultCount = recipe->result.count});
    }
    sendCompressed(play::UpdateRecipesPacket::kId, [&options](wire::io::ByteBuffer& buffer) {
        play::UpdateRecipesPacket{.stonecutter = options}.write(buffer);
    });
}

void ClientSession::broadcastChat(const std::string& message) {
    PlayerSession* session = playerSession_.get();
    if (session == nullptr) {
        return;
    }
    std::string spoken = message;
    if (options_.plugins.chatAllowed && !options_.plugins.chatAllowed(*session, spoken)) {
        return;
    }
    // The console line is `<Name> message`, which is what vanilla logs: its own chat type decorates
    // the message the same way before it reaches the log.
    const wire::text::Component text =
        wire::text::Component::text("<" + session->name() + "> " + spoken).color(wire::text::kWhite);
    logger_.info([this, &text] { return wire::text::resolvePlainText(text, consoleLang()); });
    if (options_.playerRegistry == nullptr) {
        return;
    }
    // Through the connection's own way of saying something rather than through a Java packet: a
    // Bedrock player is in this registry too, and that edition writes its chat differently. For a
    // Java player the two are the same call.
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

void ClientSession::runCommand(const std::string& command) {
    PlayerSession* session = playerSession_.get();
    command::CommandDispatcher* dispatcher = options_.commands;
    if (session == nullptr || dispatcher == nullptr) {
        return;
    }
    std::string line = command;
    if (options_.plugins.commandAllowed && !options_.plugins.commandAllowed(*session, line)) {
        return;
    }
    // No vanilla counterpart: vanilla logs only the result of a command. It is logged anyway because
    // an operator watching the console should see what players are running, and console output is
    // not part of protocol parity.
    logger_.info([session, &line] { return session->name() + " issued server command: /" + line; });
    command::PlayerCommandSource source{*session, dispatcher->environment().groups()};
    dispatcher->execute(source, line);
}

void ClientSession::sendSuggestions(const play::CommandSuggestionRequestPacket& request) {
    PlayerSession* session = playerSession_.get();
    command::CommandDispatcher* dispatcher = options_.commands;
    if (session == nullptr || dispatcher == nullptr) {
        return;
    }
    command::PlayerCommandSource source{*session, dispatcher->environment().groups()};
    const command::SuggestionResult result = dispatcher->suggest(source, request.command);
    sendPlayPacket(play::CommandSuggestionsPacket::kId, [&request, &result](wire::io::ByteBuffer& buffer) {
        play::CommandSuggestionsPacket{
            .id = request.id, .start = result.start, .length = result.length, .suggestions = result.suggestions}
            .write(buffer);
    });
}

player::PlayerConnection ClientSession::playerConnection() {
    player::PlayerConnection connection;
    connection.protocolVersion = handshake_.has_value() ? handshake_->protocolVersion : 0;
    connection.connectedHost = handshake_.has_value() ? handshake_->hostName : "";
    connection.address = remoteAddress();
    world::RealEnchantments* enchantments = options_.enchantments;
    connection.enchantmentIds = [enchantments](const Identifier& name) {
        return enchantments == nullptr ? std::nullopt : enchantments->protocolIdOf(name);
    };
    connection.sendSystemMessage = [this](const wire::text::Component& message) { sendPluginText(message, false); };
    connection.sendPlayPacket = [this](std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& write) {
        sendPlayPacket(id, write);
    };
    connection.broadcastEffects = [this](std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                                         const std::vector<world::BlockEffect>& effects) {
        blockInteraction_->broadcastEffects(play::BlockPos{x, y, z}, stateId, effects);
    };
    connection.resendChunk = [this](std::int32_t chunkX, std::int32_t chunkZ) { resendChunk(chunkX, chunkZ); };
    connection.handOverToRegion = [this](world::LoadedWorld& world) { handOverToRegionOf(world); };
    connection.sendBiomes = [this](const std::vector<std::pair<std::int32_t, std::int32_t>>& columns) {
        sendBiomes(columns);
    };
    connection.sendWeather = [this](const world::WorldWeather& weather) { sendWeather(weather); };
    connection.disconnect = [this](const wire::text::Component& reason) { disconnect(reason); };
    connection.setGamemode = [this](play::Gamemode mode) { setGamemode(mode); };
    connection.sendInventoryAll = [this] { sendInventoryAll(); };
    connection.heal = [this](float amount) { heal(amount); };
    connection.hurt = [this](float amount, std::string_view deathMessageId, const Identifier& damageType,
                             const std::optional<entity::DamageSource>& source) {
        hurt(amount, deathMessageId, damageType, source);
    };
    connection.addEffect = [this](const Identifier& effect, std::int32_t durationTicks, std::int32_t amplifier) {
        addEffect(effect, durationTicks, amplifier);
    };
    connection.sendEffect = [this](const world::ActiveEffect& effect) { uiState_->sendEffect(effect); };
    connection.sendEffectRemoved = [this](const Identifier& effect) { uiState_->sendEffectRemoved(effect); };
    connection.switchWorld = [this](world::LoadedWorld& target, double x, double y, double z) {
        return switchWorld(target, x, y, z);
    };
    connection.teleportTo = [this](double x, double y, double z, float yaw, float pitch) {
        teleportTo(x, y, z, yaw, pitch);
    };
    connection.wakeUp = [this] { wakeUp(); };
    connection.sendTime = [this](const world::LoadedWorld& world) { uiState_->sendTime(world); };
    connection.sendActionBar = [this](const wire::text::Component& message) { sendPluginText(message, true); };
    connection.sendTitle = [this](const wire::text::Component& title, const wire::text::Component& subtitle,
                                  std::int32_t fadeIn, std::int32_t stay, std::int32_t fadeOut) {
        // The timings first: sent after the text, the client applies them to the *next* title.
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
    connection.syncSubmersion = [this] { syncSubmersion(); };
    connection.sendCommandTree = [this] { sendCommandTree(); };
    connection.syncExperience = [this] { syncExperience(); };
    connection.killPlayer = [this](std::string_view deathMessageId) { killPlayer(deathMessageId); };
    connection.storedSnapshot = [this] { return storedSnapshot(); };
    // A window a plugin drew. The controller has had all three of these all along; without them
    // `menu:open` found no way through and returned as if it had opened one.
    connection.openPluginMenu = [this](const std::string& title, std::int32_t rows,
                                       const std::map<std::int32_t, server::plugins::PluginItem>& items,
                                       std::function<void(std::int32_t)> onClick) {
        menus_->openPluginMenu(title, rows, menuItemsOf(items), std::move(onClick));
    };
    connection.refreshPluginMenu = [this](const std::map<std::int32_t, server::plugins::PluginItem>& items) {
        menus_->refreshPluginMenu(menuItemsOf(items));
    };
    connection.closeOpenMenu = [this] { menus_->closeOpenMenu(); };
    return connection;
}

void ClientSession::enterPlay() {
    state_ = wire::protocol::ProtocolState::Play;
    player::PlayerRegistry* registry = options_.playerRegistry;
    if (registry == nullptr || !pendingProfile_.has_value()) {
        connection_.close();
        return;
    }
    const std::int32_t entityId = registry->allocateEntityId();

    // Which world this player belongs to is decided before anything is sent, because the login
    // packet already carries a position. A stored world that is no longer declared is not an error:
    // the player goes to the default world and the log says so.
    const std::optional<storage::StoredPlayer> stored =
        options_.playerStore == nullptr || !options_.playerStore->isOpen()
            ? std::nullopt
            : options_.playerStore->players().load(pendingProfile_->id.toString());
    const auto requested = stored.has_value() ? std::optional{stored->world} : std::nullopt;
    world::LoadedWorld* world = options_.defaultWorld;
    if (requested.has_value()) {
        const auto found = options_.worlds.find(*requested);
        if (found != options_.worlds.end()) {
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
    // Before the login packet rather than after it, because everything below this point describes
    // this player and so needs them to exist.
    auto session = std::make_unique<PlayerSession>(pendingProfile_->id, pendingProfile_->name, entityId,
                                                   playerConnection(), options_.defaultGamemode);
    session->x = spawn[0];
    session->y = spawn[1];
    session->z = spawn[2];
    session->flying = options_.defaultGamemode == play::Gamemode::Spectator;
    session->worldName = world->name();
    // Before a single packet goes out: the login packet carries the game mode and the position, so
    // restoring afterwards would mean announcing one state and then correcting it.
    if (stored.has_value()) {
        player::playerPersistence::restore(*session, *stored);
        firstSeenEpochSeconds = stored->firstSeenEpochSeconds;
        playTimeTicks = stored->playTimeTicks;
        // Only the *position* is meaningless when the stored world is gone. Health, hunger,
        // experience and game mode are the player's, not the world's.
        if (stored->world != world->name()) {
            session->x = spawn[0];
            session->y = spawn[1];
            session->z = spawn[2];
        }
        logger_.info([&session] { return "Loaded " + session->name() + " from players.db."; });
    }
    session->profileProperties.clear();
    for (const wire::auth::SessionProfileProperty& property : pendingProfile_->properties) {
        session->profileProperties.push_back(wire::protocol::login::GameProfileProperty{
            .name = property.name, .value = property.value, .signature = property.signature});
    }
    session->skinParts = pendingSkinParts_;
    playerSession_ = std::move(session);
    PlayerSession& player = *playerSession_;

    std::vector<std::string> worldNames;
    worldNames.reserve(options_.worlds.size());
    for (const auto& [name, loaded] : options_.worlds) {
        worldNames.push_back(loaded->levelKey());
    }
    const bool online = options_.onlineMode != nullptr;
    sendCompressed(play::LoginPlayPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        play::LoginPlayPacket{.entityId = entityId,
                              .isHardcore = false,
                              // Every world, so the client accepts being moved into any of them.
                              .worldNames = worldNames,
                              .maxPlayers = 20,
                              .viewDistance = chunkRadius,
                              .simulationDistance = chunkRadius,
                              .reducedDebugInfo = false,
                              .enableRespawnScreen = true,
                              .doLimitedCrafting = false,
                              // The session's mode, not the configured default: a returning player
                              // was restored above.
                              .spawnInfo = spawnInfo(player.gamemode),
                              .onlineMode = online,
                              .enforcesSecureChat = false}
            .write(buffer);
    });

    // Straight after the login packet, where vanilla's own join sequence puts them: abilities decide
    // whether the client will even let itself fly, and without a command tree its own parser rejects
    // every slash command before one reaches the server.
    uiState_->sendAbilities();
    // parity: the recipes go right here, between the abilities and the permission level. The
    // stonecutter draws its own buttons from them.
    sendStonecutterRecipes();
    uiState_->sendCommandTree();

    // Without these a real client's "loading terrain" screen never dismisses: it has no centre to
    // count chunks around and no radius to know when it has enough.
    const server::world::ChunkTracker::Delta initialChunks =
        chunkStreamer_->initialJoin(player.x, player.z, chunkRadius);

    // parity: teleport and player info before level data, and the chunks last. Chunks first leaves a
    // real client on "loading terrain" for ever.
    uiState_->sendTime(*world);
    if (world->weather().raining) {
        uiState_->sendWeather(world->weather());
    }
    // parity: `CustomBossEvents.onPlayerConnect`, which shows a joining player every bar that names
    // them. Without it a `/bossbar` set before they joined is invisible until it changes.
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
    sendCompressed(play::PlayerPositionPacket::kId, [&player](wire::io::ByteBuffer& buffer) {
        play::PlayerPositionPacket{
            .teleportId = 0, .x = player.x, .y = player.y, .z = player.z, .yaw = player.yaw, .pitch = player.pitch}
            .write(buffer);
    });

    const std::vector<PlayerSession*> others = registry->snapshot();
    registry->add(&player);
    // Their advancements and recipe book, before anything can ask about them: `limited_crafting`
    // consults the book on the first click.
    if (options_.progress != nullptr) {
        options_.progress->load(player.uuid().toString());
    }
    logger_.info([&player, entityId] {
        return player.name() + " logged in with entity id " + std::to_string(entityId) + " at (" +
               oneDecimal(player.x) + ", " + oneDecimal(player.y) + ", " + oneDecimal(player.z) + ")";
    });
    logger_.info([&player] { return player.name() + " joined the game"; });
    if (options_.plugins.joined) {
        // Handed to the owner rather than announced from here, which is this connection's own
        // thread: a decider only runs on the thread plugins live on, and fired from here the host
        // drops it and warns. Nothing waits for the answer, so a turn later is late enough.
        onOwner([this] {
            const PlayerSession* joined = playerSession_.get();
            if (joined != nullptr) {
                options_.plugins.joined(*joined);
            }
        });
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

    // Their own entity, not just everyone else's: a client reads its *own* skin layers from the
    // synced metadata, so without this the player sees themselves with no hat, jacket or sleeves.
    const play::SetEntityDataPacket skin = play::SetEntityDataPacket::skinParts(entityId, player.skinParts);
    sendCompressed(play::SetEntityDataPacket::kId, [&skin](wire::io::ByteBuffer& buffer) { skin.write(buffer); });

    // The HUD draws no hearts and no experience bar until it has been told what to draw.
    uiState_->syncHealth();
    uiState_->syncExperience();
    // parity: the inventory goes here, after the level info and the player list, not among the
    // chunk-radius packets above. Order matters on join.
    sendInventory();

    // Awaits and sends the ground the player lands on, then queues the rest of the view distance.
    chunkStreamer_->sendInitialChunks(initialChunks);

    // Only the players who cannot see this one as an entity: for the rest, the entity sync sends the
    // entry itself, immediately before the spawn, so the two never arrive out of order.
    const play::PlayerInfoUpdatePacket joining = play::PlayerInfoUpdatePacket::initializing({entryFor(player)});
    for (const PlayerSession* other : others) {
        if (other->worldName == player.worldName || !other->connection().sendPlayPacket) {
            continue;
        }
        other->connection().sendPlayPacket(play::PlayerInfoUpdatePacket::kId,
                                           [&joining](wire::io::ByteBuffer& buffer) { joining.write(buffer); });
    }

    // Drawn here rather than left to the outbound loop, which sends chunks before it redraws
    // anything: on a fresh join the scoreboard, tab list and boss bar would wait behind the whole
    // terrain stream.
    uiState_->tickUi();

    // parity: announced here, near the end and in yellow, not on the first packet of the join.
    // Everyone means everyone on the server, not just this world.
    broadcastToEveryone(
        wire::text::Component::translatable("multiplayer.player.joined", {wire::text::Component::text(player.name())})
            .color(wire::text::kYellow));

    // Last, once the whole join sequence is on the wire. Started any earlier and this thread
    // interleaves chunk batches with the packets still finishing the join, which a client reads as a
    // protocol desynchronisation.
    startOutboundLoop();
}

void ClientSession::handlePlay(std::int32_t packetId, wire::io::ByteBuffer& body) {
    PlayerSession* session = playerSession_.get();
    if (packetId == play::ServerboundPositionPacket::kId) {
        const std::optional<play::ServerboundPositionPacket> move = play::ServerboundPositionPacket::read(body);
        if (move.has_value()) {
            updatePosition(move->x, move->y, move->z, session == nullptr ? 0.0F : session->yaw,
                           session == nullptr ? 0.0F : session->pitch, move->onGround);
        }
        return;
    }
    if (packetId == play::ServerboundPositionLookPacket::kId) {
        const std::optional<play::ServerboundPositionLookPacket> move = play::ServerboundPositionLookPacket::read(body);
        if (move.has_value()) {
            updatePosition(move->x, move->y, move->z, move->yaw, move->pitch, move->onGround);
        }
        return;
    }
    // parity: `ServerboundMovePlayerPacket.Rot`, which a client sends when it turns on the spot.
    if (packetId == kMovePlayerRotId) {
        const float yaw = body.readFloat();
        const float pitch = body.readFloat();
        const bool onGround = (body.readByte() & 1) != 0;
        if (session != nullptr && !body.failed()) {
            updatePosition(session->x, session->y, session->z, yaw, pitch, onGround);
        }
        return;
    }
    // parity: `ServerboundMovePlayerPacket.StatusOnly`: neither moved nor turned, only landed.
    if (packetId == kMovePlayerStatusOnlyId) {
        const bool onGround = (body.readByte() & 1) != 0;
        if (session != nullptr && !body.failed()) {
            updatePosition(session->x, session->y, session->z, session->yaw, session->pitch, onGround);
        }
        return;
    }
    if (packetId == play::ChatMessagePacket::kId) {
        const std::optional<play::ChatMessagePacket> chat = play::ChatMessagePacket::read(body);
        if (chat.has_value()) {
            broadcastChat(chat->message);
        }
        return;
    }
    if (packetId == play::ChatCommandPacket::kId) {
        const std::optional<play::ChatCommandPacket> command = play::ChatCommandPacket::read(body);
        if (command.has_value()) {
            runCommand(command->command);
        }
        return;
    }
    // A command carrying signed arguments. Nothing verifies the signatures yet, so it runs exactly
    // like its unsigned form.
    if (packetId == play::SignedChatCommandPacket::kId) {
        const std::optional<play::SignedChatCommandPacket> command = play::SignedChatCommandPacket::read(body);
        if (command.has_value()) {
            runCommand(command->command);
        }
        return;
    }
    if (packetId == play::CommandSuggestionRequestPacket::kId) {
        const std::optional<play::CommandSuggestionRequestPacket> request =
            play::CommandSuggestionRequestPacket::read(body);
        if (request.has_value()) {
            sendSuggestions(*request);
        }
        return;
    }
    if (packetId == play::ClientCommandPacket::kId) {
        const std::optional<play::ClientCommandPacket> command = play::ClientCommandPacket::read(body);
        if (!command.has_value()) {
            return;
        }
        if (command->action == play::ClientCommandPacket::Action::PerformRespawn) {
            lifecycle_->respawn();
        } else if (command->action == play::ClientCommandPacket::Action::RequestStats) {
            uiState_->sendStats();
        }
        return;
    }
    if (packetId == play::ServerboundPlayerAbilitiesPacket::kId) {
        // The client toggles its own flight and tells the server after the fact. Believed only when
        // the game mode actually allows it.
        const std::optional<play::ServerboundPlayerAbilitiesPacket> abilities =
            play::ServerboundPlayerAbilitiesPacket::read(body);
        if (session != nullptr && abilities.has_value()) {
            session->flying = abilities->flying && session->mayFly();
        }
        return;
    }
    if (packetId == play::PlayerCommandPacket::kId) {
        const std::optional<play::PlayerCommandPacket> command = play::PlayerCommandPacket::read(body);
        if (!command.has_value()) {
            return;
        }
        if (command->action == play::PlayerCommandPacket::Action::StopSleeping) {
            wakeUp();
        } else if (command->action == play::PlayerCommandPacket::Action::StartSprinting && session != nullptr) {
            if (options_.plugins.sprintChanged) {
                options_.plugins.sprintChanged(*session, true);
            }
            session->sprinting = true;
        } else if (command->action == play::PlayerCommandPacket::Action::StopSprinting && session != nullptr) {
            if (options_.plugins.sprintChanged) {
                options_.plugins.sprintChanged(*session, false);
            }
            session->sprinting = false;
        }
        return;
    }
    if (packetId == play::ServerboundPlayerActionPacket::kId) {
        const std::optional<play::ServerboundPlayerActionPacket> action =
            play::ServerboundPlayerActionPacket::read(body);
        if (action.has_value()) {
            blockInteraction_->handlePlayerAction(*action);
        }
        return;
    }
    if (packetId == play::ServerboundSetCarriedItemPacket::kId) {
        const std::optional<play::ServerboundSetCarriedItemPacket> carried =
            play::ServerboundSetCarriedItemPacket::read(body);
        if (session != nullptr && carried.has_value() && carried->slot >= 0 &&
            carried->slot < PlayerInventory::kHotbarSize) {
            if (options_.plugins.heldSlotChanged) {
                options_.plugins.heldSlotChanged(*session, carried->slot, session->inventory.heldSlot());
            }
            session->inventory.setHeldSlot(carried->slot);
            uiState_->syncHeldAttributes();
        }
        return;
    }
    if (packetId == play::ServerboundSetCreativeModeSlotPacket::kId) {
        const std::optional<play::ServerboundSetCreativeModeSlotPacket> packet =
            play::ServerboundSetCreativeModeSlotPacket::read(body);
        if (packet.has_value()) {
            menus_->handleSetCreativeModeSlot(*packet);
        }
        return;
    }
    if (packetId == play::ServerboundContainerClickPacket::kId) {
        const std::optional<play::ServerboundContainerClickPacket> packet =
            play::ServerboundContainerClickPacket::read(body);
        if (packet.has_value()) {
            menus_->handleContainerClick(*packet);
        }
        return;
    }
    if (packetId == play::CustomPayloadPacket::kServerboundId) {
        handleCustomPayload(body);
        return;
    }
    if (packetId == play::ContainerClosePacket::kServerboundId) {
        static_cast<void>(play::ContainerClosePacket::read(body));
        if (session != nullptr) {
            if (options_.plugins.menuClosed) {
                options_.plugins.menuClosed(*session, menus_->openMenu.containerId() == 0 ? "player" : "container");
            }
            menus_->closeOpenMenu(*session);
        }
        return;
    }
    if (packetId == play::InteractPacket::kId) {
        const std::optional<play::InteractPacket> packet = play::InteractPacket::read(body);
        if (packet.has_value()) {
            combat_->handleInteract(*packet);
        }
        return;
    }
    if (packetId == play::AttackPacket::kId) {
        const std::optional<play::AttackPacket> packet = play::AttackPacket::read(body);
        if (packet.has_value()) {
            combat_->handleAttack(*packet);
        }
        return;
    }
    if (packetId == play::SelectTradePacket::kId) {
        const std::optional<play::SelectTradePacket> packet = play::SelectTradePacket::read(body);
        if (packet.has_value()) {
            trades_->selectTrade(packet->index);
        }
        return;
    }
    if (packetId == play::ContainerButtonClickPacket::kId) {
        const std::optional<play::ContainerButtonClickPacket> packet = play::ContainerButtonClickPacket::read(body);
        if (packet.has_value()) {
            menus_->clickContainerButton(packet->buttonId);
        }
        return;
    }
    if (packetId == play::ContainerSlotStateChangedPacket::kId) {
        const std::optional<play::ContainerSlotStateChangedPacket> packet =
            play::ContainerSlotStateChangedPacket::read(body);
        if (packet.has_value()) {
            menus_->setSlotState(packet->slotId, packet->newState);
        }
        return;
    }
    if (packetId == play::SetCommandBlockPacket::kId) {
        const std::optional<play::SetCommandBlockPacket> packet = play::SetCommandBlockPacket::read(body);
        if (packet.has_value()) {
            blockInteraction_->programCommandBlock(*packet);
        }
        return;
    }
    if (packetId == play::SignUpdatePacket::kId) {
        const std::optional<play::SignUpdatePacket> packet = play::SignUpdatePacket::read(body);
        if (packet.has_value()) {
            blockInteraction_->writeSign(*packet);
        }
        return;
    }
    if (packetId == kSwingPacketId) {
        blockInteraction_->handleSwing(wire::protocol::varInt::read(body));
        return;
    }
    if (packetId == play::ServerboundUseItemOnPacket::kId) {
        const std::optional<play::ServerboundUseItemOnPacket> packet = play::ServerboundUseItemOnPacket::read(body);
        if (packet.has_value()) {
            blockInteraction_->handleUseItemOn(*packet);
        }
        return;
    }
    if (packetId == play::ServerboundUseItemPacket::kId) {
        const std::optional<play::ServerboundUseItemPacket> packet = play::ServerboundUseItemPacket::read(body);
        if (packet.has_value()) {
            blockInteraction_->handleUseItem(*packet);
        }
        return;
    }
    if (packetId == play::PickItemFromEntityPacket::kId) {
        const std::optional<play::PickItemFromEntityPacket> packet = play::PickItemFromEntityPacket::read(body);
        if (packet.has_value()) {
            menus_->handlePickItemFromEntity(*packet);
        }
        return;
    }
    if (packetId == play::PickItemFromBlockPacket::kId) {
        const std::optional<play::PickItemFromBlockPacket> packet = play::PickItemFromBlockPacket::read(body);
        if (packet.has_value()) {
            menus_->handlePickItemFromBlock(*packet);
        }
        return;
    }
    if (packetId == kServerboundClientInformationId) {
        // parity: `ServerPlayer.updateOptions`, which re-reads this every time the player touches
        // their video settings, not just once at login.
        const std::optional<wire::protocol::configuration::ClientInformationPacket> information =
            wire::protocol::configuration::ClientInformationPacket::read(body);
        if (!information.has_value()) {
            return;
        }
        const auto skinParts = static_cast<std::int8_t>(information->skinParts);
        pendingSkinParts_ = skinParts;
        if (session != nullptr && session->skinParts != skinParts) {
            session->skinParts = skinParts;
            const play::SetEntityDataPacket metadata =
                play::SetEntityDataPacket::skinParts(session->entityId(), skinParts);
            sendPlayPacket(play::SetEntityDataPacket::kId,
                           [&metadata](wire::io::ByteBuffer& buffer) { metadata.write(buffer); });
            for (const PlayerSession* other : playersInSameWorld()) {
                if (other->uuid() == session->uuid() || !other->connection().sendPlayPacket) {
                    continue;
                }
                other->connection().sendPlayPacket(
                    play::SetEntityDataPacket::kId,
                    [&metadata](wire::io::ByteBuffer& buffer) { metadata.write(buffer); });
            }
        }
        return;
    }
    if (packetId == play::ChunkBatchReceivedPacket::kId) {
        // The client's own answer to "how fast can you take chunks". This is the protocol's flow
        // control; it is not advice.
        const std::optional<play::ChunkBatchReceivedPacket> packet = play::ChunkBatchReceivedPacket::read(body);
        if (packet.has_value()) {
            chunkStreamer_->desiredChunksPerTick =
                std::clamp(packet->desiredChunksPerTick, kMinChunksPerTick, kMaxChunksPerTick);
        }
        return;
    }
    // `player_input`: the movement keys currently held, sent on every change. Sneaking lives here
    // now, and it decides whether right-clicking a crafting table opens it or places against it.
    if (packetId == kServerboundPlayerInputId) {
        const std::int32_t inputs = body.readByte();
        if (session == nullptr || body.failed()) {
            return;
        }
        if (options_.plugins.sneakChanged && ((inputs & kSneakInputFlag) != 0) != session->sneaking) {
            options_.plugins.sneakChanged(*session, (inputs & kSneakInputFlag) != 0);
        }
        session->sneaking = (inputs & kSneakInputFlag) != 0;
        session->inputForward = (inputs & kForwardInputFlag) != 0;
        session->inputBack = (inputs & kBackInputFlag) != 0;
        session->inputLeft = (inputs & kLeftInputFlag) != 0;
        session->inputRight = (inputs & kRightInputFlag) != 0;
        // parity: `Player.wantsToStopRiding`, which is the crouch key and nothing else.
        if (session->sneaking && session->vehicleId.has_value()) {
            dismount(*session);
        }
    }
}

} // namespace basalt::server::runtime
