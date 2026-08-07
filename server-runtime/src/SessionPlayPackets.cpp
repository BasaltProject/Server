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
#include <basalt/server/runtime/item/ItemLimits.hpp>
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

void ClientSession::handlePlay(std::int32_t packetId, wire::io::ByteBuffer& body) {
    PlayerSession* session = playerSession_.get();
    if (session != nullptr && !kPassivePackets.contains(packetId)) {
        session->extras.lastActionMillis = steadyMillis();
    }
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
            const bool wants = abilities->flying && session->mayFly();
            if (wants != session->flying && options_.events != nullptr &&
                !options_.events->allowed(&server::plugins::pluginEventTypes::playerToggleFlight,
                                          [&](server::plugins::PluginEvent& event) {
                                              plugins::EventSink::player(event, *session);
                                              event.set("flying", wants);
                                          })) {
                uiState_->sendAbilities();
                return;
            }
            session->flying = wants;
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
            menus_->closeOpenMenu(*session, "player");
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
        if (packet.has_value() && session != nullptr && options_.limits != nullptr) {
            const config::ItemCaps& caps = options_.limits->items;
            const bool longPage =
                caps.bookPageLength > 0 && std::ranges::any_of(packet->pages, [&caps](const std::string& page) {
                    return item::itemLimits::lengthOf(page) > caps.bookPageLength;
                });
            const bool longTitle = caps.bookTitleLength > 0 && packet->title.has_value() &&
                                   item::itemLimits::lengthOf(*packet->title) > caps.bookTitleLength;
            if (longPage || longTitle) {
                logger_.warn([session] { return session->name() + " sent a book past the limits in limits.toml"; });
                menus_->sendInventoryAll();
                return;
            }
        }
        if (packet.has_value() && session != nullptr) {
            std::vector<std::string> pages = packet->pages;
            std::optional<std::string> title = packet->title;
            if (options_.events != nullptr) {
                std::string joined;
                for (std::size_t page = 0; page < pages.size(); ++page) {
                    joined += (page == 0 ? "" : "\f") + pages[page];
                }
                const server::plugins::PluginEvent* edit = options_.events->ask(
                    &server::plugins::pluginEventTypes::playerEditBook, [&](server::plugins::PluginEvent& event) {
                        plugins::EventSink::player(event, *session);
                        event.set("slot", static_cast<std::int64_t>(packet->slot));
                        event.set("signing", title.has_value());
                        event.set("title", title.value_or(std::string{}));
                        event.set("pages", joined);
                    });
                if (edit != nullptr) {
                    if (edit->cancelled()) {
                        return;
                    }
                    if (const auto* text = std::get_if<std::string>(&edit->get("pages"));
                        text != nullptr && *text != joined) {
                        pages.clear();
                        for (std::size_t from = 0; from <= text->size();) {
                            const std::size_t end = std::min(text->find('\f', from), text->size());
                            pages.push_back(text->substr(from, end - from));
                            from = end + 1;
                        }
                    }
                    const auto* signing = std::get_if<bool>(&edit->get("signing"));
                    const auto* name = std::get_if<std::string>(&edit->get("title"));
                    if (signing != nullptr && !*signing) {
                        title.reset();
                    } else if (name != nullptr && (title.has_value() || (signing != nullptr && *signing))) {
                        title = *name;
                    }
                }
            }
            if (player::editBookFor(*session, packet->slot, pages, title)) {
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
    if (packetId == wire::protocol::configuration::ResourcePackResponsePacket::kPlayId) {
        if (const auto response = wire::protocol::configuration::ResourcePackResponsePacket::read(body);
            response.has_value()) {
            handleResourcePackResponse(*response, false);
        }
        return;
    }
    if (packetId == play::RecipeBookChangeSettingsPacket::kId) {
        const std::optional<play::RecipeBookChangeSettingsPacket> packet =
            play::RecipeBookChangeSettingsPacket::read(body);
        if (packet.has_value() && playerSession_ != nullptr && options_.progress != nullptr && packet->bookType >= 0 &&
            packet->bookType < play::kRecipeBookTypes) {
            options_.progress->setBookSetting(playerSession_->uuid().toString(), packet->bookType, packet->open,
                                              packet->filtering);
        }
        return;
    }
    if (packetId == play::RecipeBookSeenPacket::kId) {
        const std::optional<play::RecipeBookSeenPacket> packet = play::RecipeBookSeenPacket::read(body);
        if (packet.has_value() && playerSession_ != nullptr && options_.progress != nullptr &&
            options_.recipes != nullptr && packet->recipeId >= 0 &&
            packet->recipeId < static_cast<std::int32_t>(options_.recipes->ids().size())) {
            options_.progress->removeHighlight(playerSession_->uuid().toString(),
                                               *std::next(options_.recipes->ids().begin(), packet->recipeId));
        }
        return;
    }
    if (packetId == play::PlaceRecipePacket::kId) {
        const std::optional<play::PlaceRecipePacket> packet = play::PlaceRecipePacket::read(body);
        if (packet.has_value()) {
            menus_->placeRecipe(*packet);
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
    if (packetId == play::SetTestBlockPacket::kId) {
        if (const std::optional<play::SetTestBlockPacket> packet = play::SetTestBlockPacket::read(body);
            packet.has_value()) {
            blockInteraction_->setTestBlock(*packet);
        }
        return;
    }
    if (packetId == play::TestInstanceBlockActionPacket::kId) {
        if (const std::optional<play::TestInstanceBlockActionPacket> packet =
                play::TestInstanceBlockActionPacket::read(body);
            packet.has_value()) {
            blockInteraction_->testInstanceBlockAction(*packet);
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
    if (packetId == play::ServerboundCustomClickActionPacket::kId) {
        const std::optional<play::ServerboundCustomClickActionPacket> packet =
            play::ServerboundCustomClickActionPacket::read(body);
        if (packet.has_value()) {
            answerForm(*packet);
        }
        return;
    }
    if (packetId == play::kPunchId) {
        if (options_.events != nullptr && playerSession_ != nullptr &&
            !options_.events->allowed(&server::plugins::pluginEventTypes::playerAnimation,
                                      [&](server::plugins::PluginEvent& event) {
                                          plugins::EventSink::player(event, *playerSession_);
                                          event.set("hand", std::string{"main_hand"});
                                      })) {
            return;
        }
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
    if (packetId == play::PlayKeepAlivePacket::kServerboundId) {
        const std::optional<play::PlayKeepAlivePacket> answer = play::PlayKeepAlivePacket::read(body);
        if (answer.has_value() && session != nullptr && answer->keepAliveId == keepAliveToken_) {
            // parity: `ServerCommonPacketListenerImpl.handleKeepAlive`.
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastKeepAlive_)
                    .count();
            session->extras.pingMillis = (session->extras.pingMillis * 3 + static_cast<std::int32_t>(elapsed)) / 4;
        }
        return;
    }
    if (packetId == kServerboundCookieResponseId) {
        const std::string key = wire::protocol::readString(body, 256);
        const bool present = wire::protocol::readBoolean(body);
        std::vector<std::uint8_t> payload;
        if (present) {
            payload = wire::protocol::readByteArray(body, 5120);
        }
        if (session != nullptr && !body.failed()) {
            session->extras.state["cookie:" + key] =
                present ? "P" + std::string{payload.begin(), payload.end()} : std::string{"A"};
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
        if (session != nullptr) {
            session->extras.client = player::ClientOptions{.locale = information->locale,
                                                           .viewDistance = information->viewDistance,
                                                           .particleStatus = information->particleStatus,
                                                           .chatColors = information->chatColors,
                                                           .textFiltering = information->enableTextFiltering,
                                                           .serverListings = information->enableServerListing};
        }
        if (chunkStreamer_ != nullptr) {
            chunkStreamer_->setRequestedViewDistance(information->viewDistance);
        }
        if (session != nullptr && options_.events != nullptr) {
            options_.events->notify(
                &server::plugins::pluginEventTypes::playerClientOptions, [&](server::plugins::PluginEvent& event) {
                    plugins::EventSink::player(event, *session);
                    event.set("locale", information->locale);
                    event.set("view_distance", static_cast<std::int64_t>(information->viewDistance));
                    event.set("chat_colors", information->chatColors);
                    event.set("main_hand", std::string{information->mainHand == 0 ? "left" : "right"});
                    event.set("skin_parts", static_cast<std::int64_t>(information->skinParts));
                });
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
        session->extras.inputFlags = inputs;
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
