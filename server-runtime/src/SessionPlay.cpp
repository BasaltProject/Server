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

void ClientSession::answerForm(const wire::protocol::play::ServerboundCustomClickActionPacket& packet) {
    constexpr std::string_view kPrefix = "basalt:form/";
    if (!packet.id.starts_with(kPrefix)) {
        // parity: `dynamic/custom` has no server behaviour of its own; plugins are the only listeners.
        if (options_.events != nullptr && playerSession() != nullptr) {
            options_.events->notify(
                &server::plugins::pluginEventTypes::playerCustomClick, [&](server::plugins::PluginEvent& event) {
                    plugins::EventSink::player(event, *playerSession());
                    event.set("id", packet.id);
                    event.set("payload",
                              packet.payload.has_value() ? wire::nbt::snbt::stringify(*packet.payload) : std::string{});
                });
        }
        return;
    }
    if (!packet.payload.has_value()) {
        return;
    }
    const std::int32_t id = std::atoi(packet.id.c_str() + kPrefix.size());
    const wire::nbt::NbtCompound* answer = packet.payload->get<wire::nbt::NbtCompound>();
    std::shared_ptr<const world::dialogs::Dialog> dialog;
    {
        const std::lock_guard<std::mutex> guard{formsLock_};
        const auto found = forms_.find(id);
        if (found == forms_.end() || answer == nullptr) {
            return;
        }
        dialog = std::move(found->second);
        forms_.erase(found);
    }
    const wire::nbt::NbtTag* button = answer->find(world::dialogs::kButtonKey);
    const auto* index = button == nullptr ? nullptr : button->get<std::int32_t>();
    if (index == nullptr || *index < 0 || static_cast<std::size_t>(*index) >= dialog->buttons.size() ||
        !dialog->submit) {
        return;
    }
    std::map<std::string, std::string> values;
    for (const world::dialogs::Input& input : dialog->inputs) {
        const wire::nbt::NbtTag* held = answer->find(input.key);
        if (held == nullptr) {
            continue;
        }
        if (const auto* text = held->get<std::string>(); text != nullptr) {
            values[input.key] = *text;
        } else if (const auto* flag = held->get<std::int8_t>(); flag != nullptr) {
            values[input.key] = *flag != 0 ? input.onTrue : input.onFalse;
        } else if (const auto* number = held->get<float>(); number != nullptr) {
            values[input.key] = world::dialogs::numberText(*number);
        }
    }
    // Plugins run on the owner's thread, and the answer arrives on the one that reads this connection.
    onOwner([dialog, button = static_cast<std::size_t>(*index), values] { dialog->submit(button, values); });
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
    if (options_.events != nullptr && options_.events->listening(&server::plugins::pluginEventTypes::commandUnknown)) {
        const std::string word = line.substr(0, line.find(' '));
        const std::vector<command::CommandNode*> usable = dispatcher->usableCommands(source);
        if (std::ranges::none_of(usable, [&word](const command::CommandNode* node) { return node->name() == word; })) {
            const server::plugins::PluginEvent* asked = options_.events->ask(
                &server::plugins::pluginEventTypes::commandUnknown, [&](server::plugins::PluginEvent& event) {
                    plugins::EventSink::player(event, *session);
                    event.set("command", line);
                    event.set("message", std::string{});
                    event.set("silent", false);
                });
            const auto* message = asked == nullptr ? nullptr : std::get_if<std::string>(&asked->get("message"));
            const auto* silent = asked == nullptr ? nullptr : std::get_if<bool>(&asked->get("silent"));
            if (silent != nullptr && *silent) {
                return;
            }
            if (message != nullptr && !message->empty()) {
                source.sendSystemMessage(wire::text::miniMessage::parse(*message));
                return;
            }
        }
    }
    dispatcher->execute(source, line);
}

void ClientSession::sendSuggestions(const play::CommandSuggestionRequestPacket& request) {
    PlayerSession* session = playerSession_.get();
    command::CommandDispatcher* dispatcher = options_.commands;
    if (session == nullptr || dispatcher == nullptr) {
        return;
    }
    command::PlayerCommandSource source{*session, dispatcher->environment().groups()};
    command::SuggestionResult result = dispatcher->suggest(source, request.command);
    if (options_.events != nullptr) {
        std::string joined;
        for (const wire::protocol::play::CommandSuggestion& suggestion : result.suggestions) {
            joined += (joined.empty() ? "" : "\n") + suggestion.text;
        }
        const server::plugins::PluginEvent* asked = options_.events->ask(
            &server::plugins::pluginEventTypes::playerTabComplete, [&](server::plugins::PluginEvent& event) {
                plugins::EventSink::player(event, *session);
                event.set("buffer", request.command);
                event.set("completions", joined);
            });
        if (asked != nullptr) {
            if (asked->cancelled()) {
                result.suggestions.clear();
            } else if (const auto* edited = std::get_if<std::string>(&asked->get("completions"));
                       edited != nullptr && *edited != joined) {
                result.suggestions.clear();
                for (std::size_t from = 0; from < edited->size();) {
                    const std::size_t end = std::min(edited->find('\n', from), edited->size());
                    if (end > from) {
                        result.suggestions.push_back(
                            {.text = edited->substr(from, end - from), .tooltip = std::nullopt});
                    }
                    from = end + 1;
                }
            }
        }
    }
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

} // namespace basalt::server::runtime
