#include <basalt/server/registry/BlockSupport.hpp>
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
#include <basalt/server/runtime/player/PlayerBooks.hpp>
#include <basalt/server/runtime/player/PlayerGliding.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerPersistence.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/Dialogs.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/PlayerAdvancements.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
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
#include <basalt/wire/text/Serializers.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <expected>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime {
namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;

using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;

/// parity: `ServerboundMovePlayerPacket`.
constexpr std::int32_t kMovePlayerRotId = 0x20;
constexpr std::int32_t kMovePlayerStatusOnlyId = 0x21;
constexpr std::int32_t kServerboundClientInformationId = 0x0e;
constexpr std::int32_t kServerboundPlayerInputId = 0x2b;

// parity: `Input`'s own bit order.
constexpr std::int32_t kForwardInputFlag = 0x01;
constexpr std::int32_t kBackInputFlag = 0x02;
constexpr std::int32_t kLeftInputFlag = 0x04;
constexpr std::int32_t kRightInputFlag = 0x08;
constexpr std::int32_t kSneakInputFlag = 0x20;

constexpr float kMinChunksPerTick = 0.01F;
constexpr float kMaxChunksPerTick = 64.0F;

std::string oneDecimal(double value) {
    // The sign is kept apart: dividing a negative tenth by ten is zero, so values in (-1, 0) looked positive.
    const auto tenths = static_cast<std::int64_t>(std::llround(std::abs(value) * 10.0));
    const std::string sign = value < 0.0 && tenths != 0 ? "-" : "";
    return sign + std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

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
                                 .properties = player.profileProperties,
                                 .chatSession = player.chatSession()};
}

[[nodiscard]] std::vector<std::int32_t> itemIdsOf(const std::vector<const server::world::crafting::Ingredient*>& from) {
    std::set<std::int32_t> ids;
    for (const server::world::crafting::Ingredient* ingredient : from) {
        for (const std::string& item : ingredient->items) {
            const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(item);
            if (const reg::Item* entry = id.has_value() ? reg::items::registry().byName(*id) : nullptr;
                entry != nullptr) {
                ids.insert(entry->protocolId);
            }
        }
    }
    return {ids.begin(), ids.end()};
}

/// parity: `RecipeManager`.
[[nodiscard]] std::vector<play::ItemPropertySet> propertySetsOf(const world::RealRecipes& recipes) {
    namespace crafting = server::world::crafting;
    using Pick = const crafting::Ingredient* (*)(const crafting::Recipe&);
    const auto set = [](std::string_view key, const std::vector<const crafting::Recipe*>& from, Pick pick) {
        std::vector<const crafting::Ingredient*> ingredients;
        for (const crafting::Recipe* recipe : from) {
            if (const crafting::Ingredient* ingredient = pick(*recipe); ingredient != nullptr) {
                ingredients.push_back(ingredient);
            }
        }
        return play::ItemPropertySet{.key = std::string{key}, .itemIds = itemIdsOf(ingredients)};
    };
    const Pick input = [](const crafting::Recipe& recipe) { return &recipe.ingredient; };
    std::vector<play::ItemPropertySet> sets;
    sets.push_back(set("minecraft:smithing_base", recipes.smithing(),
                       [](const crafting::Recipe& recipe) { return &recipe.base; }));
    sets.push_back(set("minecraft:smithing_template", recipes.smithing(),
                       [](const crafting::Recipe& recipe) { return &recipe.smithingTemplate; }));
    sets.push_back(set("minecraft:smithing_addition", recipes.smithing(),
                       [](const crafting::Recipe& recipe) { return &recipe.addition; }));
    sets.push_back(set("minecraft:furnace_input", recipes.cooking(crafting::Cooker::Smelting), input));
    sets.push_back(set("minecraft:blast_furnace_input", recipes.cooking(crafting::Cooker::Blasting), input));
    sets.push_back(set("minecraft:smoker_input", recipes.cooking(crafting::Cooker::Smoking), input));
    sets.push_back(set("minecraft:campfire_input", recipes.cooking(crafting::Cooker::Campfire), input));
    sets.push_back(
        set("minecraft:brewing_input", recipes.brewing(), [](const crafting::Recipe& recipe) { return &recipe.base; }));
    sets.push_back(set("minecraft:brewing_reagent", recipes.brewing(), input));
    return sets;
}

} // namespace

void ClientSession::sendStonecutterRecipes() {
    world::RealRecipes* recipes = options_.recipes;
    if (recipes == nullptr) {
        return;
    }
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
    const play::UpdateRecipesPacket packet{.propertySets = propertySetsOf(*recipes), .stonecutter = std::move(options)};
    sendCompressed(play::UpdateRecipesPacket::kId, [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
}

void ClientSession::runCommand(const std::string& command, const chat::SignedArguments* signedArguments) {
    PlayerSession* session = playerSession_.get();
    command::CommandDispatcher* dispatcher = options_.commands;
    if (session == nullptr || dispatcher == nullptr) {
        return;
    }
    std::string line = command;
    if (options_.plugins.commandAllowed && !options_.plugins.commandAllowed(*session, line)) {
        return;
    }
    // No vanilla counterpart: an operator should see what players run, and console output is not protocol parity.
    logger_.info([session, &line] { return session->name() + " issued server command: /" + line; });
    command::PlayerCommandSource source{*session, dispatcher->environment().groups(), signedArguments};
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

void ClientSession::seenAdvancements(wire::io::ByteBuffer& body) {
    // parity: `handleSeenAdvancements`.
    const std::optional<play::SeenAdvancementsPacket> seen = play::SeenAdvancementsPacket::read(body);
    PlayerSession* session = playerSession_.get();
    if (!seen.has_value() || session == nullptr || options_.advancements == nullptr ||
        seen->action != play::SeenAdvancementsPacket::Action::OpenedTab || !seen->tab.has_value()) {
        return;
    }
    const std::expected<Identifier, wire::common::IdentifierError> tab = Identifier::parse(*seen->tab);
    if (!tab.has_value()) {
        return;
    }
    const std::optional<play::SelectAdvancementsTabPacket> selected =
        options_.advancements->selectTab(session->uuid().toString(), *tab);
    if (selected.has_value()) {
        sendPlayPacket(play::SelectAdvancementsTabPacket::kId,
                       [&selected](wire::io::ByteBuffer& buffer) { selected->write(buffer); });
    }
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
    connection.sendChatMessage = [this](const chat::PlayerChatMessage& message, const chat::BoundChatType& chatType) {
        deliverChat(message, chatType);
    };
    connection.sendPlayPacket = [this](std::int32_t id, const std::function<void(wire::io::ByteBuffer&)>& write) {
        sendPlayPacket(id, write);
    };
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
    connection.setGamemode = [this](play::Gamemode mode) { setGamemode(mode); };
    connection.sendInventoryAll = [this] { sendInventoryAll(); };
    connection.setHeldSlot = [this](const std::int32_t slot) {
        sendCompressed(play::SetHeldSlotPacket::kId,
                       [slot](wire::io::ByteBuffer& buffer) { play::SetHeldSlotPacket{.slot = slot}.write(buffer); });
    };
    connection.heal = [this](float amount) { heal(amount); };
    connection.hurt = [this](float amount, std::string_view deathMessageId, const Identifier& damageType,
                             const std::optional<entity::DamageSource>& source) {
        hurt(amount, deathMessageId, damageType, source);
    };
    connection.addEffect = [this](const Identifier& effect, std::int32_t durationTicks, std::int32_t amplifier) {
        addEffect(effect, durationTicks, amplifier);
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
    connection.killPlayer = [this](std::string_view deathMessageId) { killPlayer(deathMessageId); };
    connection.sendBlockChange = [this](const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                        const std::int32_t stateId) {
        const play::ClientboundBlockUpdatePacket packet{.pos = play::BlockPos{x, y, z}, .blockStateId = stateId};
        sendCompressed(play::ClientboundBlockUpdatePacket::kId,
                       [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    };
    connection.sendCombatDeath = [this](const wire::text::Component& message) {
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

    // Decided before anything is sent (the login carries a position); an undeclared stored world falls back to default.
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
    session->profileProperties.clear();
    for (const wire::auth::SessionProfileProperty& property : pendingProfile_->properties) {
        session->profileProperties.push_back(wire::protocol::login::GameProfileProperty{
            .name = property.name, .value = property.value, .signature = property.signature});
    }
    session->skinParts = pendingSkinParts_;
    session->mainArm = pendingMainHand_;
    session->chatVisibility.store(static_cast<PlayerSession::ChatVisibility>(std::clamp(pendingChatVisibility_, 0, 2)));
    playerSession_ = std::move(session);
    PlayerSession& player = *playerSession_;
    secureChat_ = std::make_unique<chat::SecureChat>(
        player.uuid(), options_.enforceSecureProfile && !translatedClient(), options_.servicesKeys);

    std::vector<std::string> worldNames;
    worldNames.reserve(options_.worlds.size());
    for (const auto& [name, loaded] : options_.worlds) {
        worldNames.push_back(loaded->levelKey());
    }
    const bool online = options_.onlineMode != nullptr;
    sendCompressed(play::LoginPlayPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        play::LoginPlayPacket{.entityId = entityId,
                              .isHardcore = false,
                              .worldNames = worldNames,
                              .maxPlayers = 20,
                              .viewDistance = chunkRadius,
                              .simulationDistance = chunkRadius,
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
    }
    if (options_.advancements != nullptr) {
        options_.advancements->join(player.uuid().toString());
    }
    if (options_.advancementTriggers != nullptr) {
        options_.advancementTriggers->grantAutomatic(player.name(), player.uuid().toString());
    }
    logger_.info([&player, entityId] {
        return player.name() + " logged in with entity id " + std::to_string(entityId) + " at (" +
               oneDecimal(player.x) + ", " + oneDecimal(player.y) + ", " + oneDecimal(player.z) + ")";
    });
    logger_.info([&player] { return player.name() + " joined the game"; });
    if (options_.plugins.joined) {
        // Handed to the owner: a decider only runs on the plugin thread, and from here the host drops it.
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

    broadcastToEveryone(
        wire::text::Component::translatable("multiplayer.player.joined", {wire::text::Component::text(player.name())})
            .color(wire::text::kYellow));

    // Last: started earlier, this thread interleaves chunk batches with join packets, which the client reads as desync.
    startOutboundLoop();
}

void ClientSession::handlePlay(std::int32_t packetId, wire::io::ByteBuffer& body) {
    PlayerSession* session = playerSession_.get();
    if (packetId == play::ServerboundPositionPacket::kId) {
        const std::optional<play::ServerboundPositionPacket> move = play::ServerboundPositionPacket::read(body);
        if (move.has_value() && !updateAwaitingTeleport()) {
            updatePosition(move->x, move->y, move->z, session == nullptr ? 0.0F : session->yaw,
                           session == nullptr ? 0.0F : session->pitch, move->onGround);
        }
        return;
    }
    if (packetId == play::ServerboundPositionLookPacket::kId) {
        const std::optional<play::ServerboundPositionLookPacket> move = play::ServerboundPositionLookPacket::read(body);
        if (move.has_value() && session != nullptr && updateAwaitingTeleport()) {
            // parity: only the rotation is taken while a teleport is unanswered.
            session->yaw = move->yaw;
            session->pitch = move->pitch;
        } else if (move.has_value()) {
            updatePosition(move->x, move->y, move->z, move->yaw, move->pitch, move->onGround);
        }
        return;
    }
    if (packetId == play::AcceptTeleportationPacket::kId) {
        const std::optional<play::AcceptTeleportationPacket> accept = play::AcceptTeleportationPacket::read(body);
        if (accept.has_value()) {
            acceptTeleport(*accept);
        }
        return;
    }
    // parity: `ServerboundMovePlayerPacket.Rot`.
    if (packetId == kMovePlayerRotId) {
        const float yaw = body.readFloat();
        const float pitch = body.readFloat();
        const bool onGround = (body.readByte() & 1) != 0;
        if (session != nullptr && !body.failed() && updateAwaitingTeleport()) {
            session->yaw = yaw;
            session->pitch = pitch;
        } else if (session != nullptr && !body.failed()) {
            updatePosition(session->x, session->y, session->z, yaw, pitch, onGround);
        }
        return;
    }
    // parity: `ServerboundMovePlayerPacket.StatusOnly`.
    if (packetId == kMovePlayerStatusOnlyId) {
        const bool onGround = (body.readByte() & 1) != 0;
        if (session != nullptr && !body.failed() && !updateAwaitingTeleport()) {
            updatePosition(session->x, session->y, session->z, session->yaw, session->pitch, onGround);
        }
        return;
    }
    if (handleChatPacket(packetId, body)) {
        return;
    }
    if (packetId == play::SeenAdvancementsPacket::kId) {
        seenAdvancements(body);
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
        // Believed only when the game mode actually allows flying.
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
        } else if (command->action == play::PlayerCommandPacket::Action::StartRidingJump ||
                   command->action == play::PlayerCommandPacket::Action::StopRidingJump ||
                   command->action == play::PlayerCommandPacket::Action::OpenInventory) {
            combat_->handleRidingCommand(command->action, command->data);
        } else if (command->action == play::PlayerCommandPacket::Action::StartFallFlying && session != nullptr) {
            player::gliding::startFallFlying(*session);
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
    if (packetId == play::ServerboundMoveVehiclePacket::kId) {
        if (const std::optional<play::ServerboundMoveVehiclePacket> packet =
                play::ServerboundMoveVehiclePacket::read(body);
            packet.has_value()) {
            combat_->handleMoveVehicle(*packet);
        }
        return;
    }
    if (packetId == play::ServerboundPaddleBoatPacket::kId) {
        if (const std::optional<play::ServerboundPaddleBoatPacket> packet =
                play::ServerboundPaddleBoatPacket::read(body);
            packet.has_value()) {
            combat_->handlePaddleBoat(*packet);
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
    if (packetId == play::ServerboundRenameItemPacket::kId) {
        const std::optional<play::ServerboundRenameItemPacket> packet = play::ServerboundRenameItemPacket::read(body);
        if (packet.has_value()) {
            menus_->setAnvilName(packet->name);
        }
        return;
    }
    if (packetId == play::ServerboundEditBookPacket::kId) {
        const std::optional<play::ServerboundEditBookPacket> packet = play::ServerboundEditBookPacket::read(body);
        if (packet.has_value() && session != nullptr) {
            if (player::editBookFor(*session, packet->slot, packet->pages, packet->title)) {
                menus_->sendInventoryAll();
            }
        }
        return;
    }
    if (packetId == play::ServerboundSetBeaconPacket::kId) {
        const std::optional<play::ServerboundSetBeaconPacket> packet = play::ServerboundSetBeaconPacket::read(body);
        if (packet.has_value()) {
            menus_->setBeaconEffects(packet->primary, packet->secondary);
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
    if (packetId == play::ServerboundSetCommandMinecartPacket::kId) {
        if (const std::optional<play::ServerboundSetCommandMinecartPacket> packet =
                play::ServerboundSetCommandMinecartPacket::read(body);
            packet.has_value()) {
            blockInteraction_->programCommandMinecart(*packet);
        }
        return;
    }
    if (packetId == play::SetStructureBlockPacket::kId) {
        if (const std::optional<play::SetStructureBlockPacket> packet = play::SetStructureBlockPacket::read(body);
            packet.has_value()) {
            blockInteraction_->setStructureBlock(*packet);
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
    if (packetId == play::kPunchId) {
        blockInteraction_->handlePunch();
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
        // parity: `ServerPlayer.updateOptions`.
        const std::optional<wire::protocol::configuration::ClientInformationPacket> information =
            wire::protocol::configuration::ClientInformationPacket::read(body);
        if (!information.has_value()) {
            return;
        }
        const auto skinParts = static_cast<std::int8_t>(information->skinParts);
        pendingSkinParts_ = skinParts;
        pendingMainHand_ = information->mainHand;
        pendingChatVisibility_ = information->chatFlags;
        pendingViewDistance_ = information->viewDistance;
        if (chunkStreamer_ != nullptr) {
            chunkStreamer_->setRequestedViewDistance(information->viewDistance);
        }
        if (session != nullptr) {
            session->chatVisibility.store(
                static_cast<PlayerSession::ChatVisibility>(std::clamp(information->chatFlags, 0, 2)));
        }
        // parity: `ServerPlayer.updateOptions` sets both, and the tracker sends what changed.
        if (session != nullptr) {
            session->skinParts = skinParts;
            session->mainArm = information->mainHand;
        }
        return;
    }
    if (packetId == play::ChunkBatchReceivedPacket::kId) {
        // This is the protocol's flow control, not advice.
        const std::optional<play::ChunkBatchReceivedPacket> packet = play::ChunkBatchReceivedPacket::read(body);
        if (packet.has_value()) {
            chunkStreamer_->desiredChunksPerTick =
                std::clamp(packet->desiredChunksPerTick, kMinChunksPerTick, kMaxChunksPerTick);
        }
        return;
    }
    // Sneaking lives here now, and it decides whether right-clicking a crafting table opens it or places against it.
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
