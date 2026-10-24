#pragma once

#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/ExperienceOrbSystem.hpp>
#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/UiStateSync.hpp>
#include <basalt/server/runtime/VillagerTradeController.hpp>
#include <basalt/server/runtime/item/ItemComponents.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Looms.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>

#include <algorithm>
#include <cstdint>
#include <random>
#include <utility>

namespace basalt::server::runtime::containerSlots {

using player::Menu;
using player::MenuSlot;
using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::BlockPos;

inline std::optional<Identifier> parsedOrNothing(const std::string& id) {
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

inline std::optional<server::world::loot::ItemStack>
craftingStackOf(const std::optional<storage::StoredItemStack>& stack) {
    if (!stack.has_value()) {
        return std::nullopt;
    }
    const std::optional<Identifier> id = parsedOrNothing(stack->id);
    if (!id.has_value()) {
        return std::nullopt;
    }
    return server::world::loot::ItemStack{.item = *id, .count = stack->count, .components = stack->components};
}

inline storage::StoredItemStack storedOf(const server::world::loot::ItemStack& stack) {
    return storage::StoredItemStack{.id = stack.item.toString(), .count = stack.count, .components = stack.components};
}

} // namespace basalt::server::runtime::containerSlots
