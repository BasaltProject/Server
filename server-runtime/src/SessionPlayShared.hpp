#pragma once

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

namespace basalt::server::runtime::sessionPlay {
namespace play = wire::protocol::play;
namespace reg = server::registry;

using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;

/// parity: `ServerboundMovePlayerPacket`.
constexpr std::int32_t kMovePlayerRotId = 0x20;
constexpr std::int32_t kMovePlayerStatusOnlyId = 0x21;
constexpr std::int32_t kServerboundClientInformationId = 0x0e;
constexpr std::int32_t kServerboundCookieResponseId = 0x15;

constexpr std::int32_t kServerboundPlayerInputId = 0x2b;

// parity: `Input`'s own bit order.
constexpr std::int32_t kForwardInputFlag = 0x01;
constexpr std::int32_t kBackInputFlag = 0x02;
constexpr std::int32_t kLeftInputFlag = 0x04;
constexpr std::int32_t kRightInputFlag = 0x08;
constexpr std::int32_t kSneakInputFlag = 0x20;

// Packets a client sends without anyone having done anything.
const std::set<std::int32_t> kPassivePackets{0x00, 0x06, 0x0b, 0x0d, 0x0e, 0x15, 0x1c, 0x1e,
                                             0x1f, 0x20, 0x21, 0x26, 0x2c, 0x2d, 0x32};

[[nodiscard]] inline std::int64_t steadyMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

constexpr float kMinChunksPerTick = 0.01F;
constexpr float kMaxChunksPerTick = 64.0F;

inline std::string oneDecimal(double value) {
    // The sign is kept apart: dividing a negative tenth by ten is zero, so values in (-1, 0) looked positive.
    const auto tenths = static_cast<std::int64_t>(std::llround(std::abs(value) * 10.0));
    const std::string sign = value < 0.0 && tenths != 0 ? "-" : "";
    return sign + std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

[[nodiscard]] inline std::map<std::int32_t, PluginMenuItem>
menuItemsOf(const std::map<std::int32_t, server::plugins::PluginItem>& items) {
    std::map<std::int32_t, PluginMenuItem> drawn;
    for (const auto& [slot, item] : items) {
        drawn.emplace(slot, PluginMenuItem{.id = item.id,
                                           .count = item.count,
                                           .name = item.name,
                                           .lore = item.lore,
                                           .components = item.components});
    }
    return drawn;
}

inline play::PlayerInfoEntry entryFor(const PlayerSession& player) {
    return play::PlayerInfoEntry{.uuid = player.uuid(),
                                 .name = player.name(),
                                 .listed = true,
                                 .gamemode = player.gamemode,
                                 .properties = player.profileProperties,
                                 .chatSession = player.chatSession()};
}

[[nodiscard]] inline std::vector<std::int32_t>
itemIdsOf(const std::vector<const server::world::crafting::Ingredient*>& from) {
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
[[nodiscard]] inline std::vector<play::ItemPropertySet> propertySetsOf(const world::RealRecipes& recipes) {
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

} // namespace basalt::server::runtime::sessionPlay
