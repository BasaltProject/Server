#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/plugins/VaultHooks.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <cstdlib>
#include <expected>
#include <string>

namespace basalt::server::runtime::plugins {
namespace {

using server::plugins::PluginEvent;
namespace types = server::plugins::pluginEventTypes;
using server::world::loot::ItemStack;

[[nodiscard]] std::string linesOf(const VaultSystem::Loot& loot) {
    std::string lines;
    for (const ItemStack& stack : loot) {
        lines += (lines.empty() ? "" : "\n") + stack.item.toString() + "|" + std::to_string(stack.count);
    }
    return lines;
}

[[nodiscard]] VaultSystem::Loot lootOf(const std::string& lines) {
    VaultSystem::Loot loot;
    for (std::size_t from = 0; from < lines.size();) {
        const std::size_t end = std::min(lines.find('\n', from), lines.size());
        const std::string line = lines.substr(from, end - from);
        from = end + 1;
        const std::size_t bar = line.find('|');
        if (bar == std::string::npos) {
            continue;
        }
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            wire::common::Identifier::parse(line.substr(0, bar));
        const std::int32_t count = std::atoi(line.c_str() + bar + 1);
        if (id.has_value() && count > 0) {
            loot.push_back(ItemStack{.item = *id, .count = count});
        }
    }
    return loot;
}

} // namespace

VaultSystem::Hooks vaultHooks(const EventSink& sink) {
    VaultSystem::Hooks hooks;
    hooks.dispenseLoot = [&sink](const player::PlayerSession& who, world::LoadedWorld& world,
                                 const VaultSystem::Pos& pos, const std::string& table, VaultSystem::Loot& loot) {
        if (!sink.listening(&types::blockDispenseLoot)) {
            return true;
        }
        const std::string lines = linesOf(loot);
        const PluginEvent* event = sink.ask(&types::blockDispenseLoot, [&](PluginEvent& filled) {
            EventSink::player(filled, who);
            hookEvents::position(filled, world.name(), pos[0], pos[1], pos[2]);
            filled.set("loot_table", table);
            filled.set("loot", lines);
        });
        if (event == nullptr) {
            return true;
        }
        if (event->cancelled()) {
            return false;
        }
        const auto* edited = std::get_if<std::string>(&event->get("loot"));
        if (edited != nullptr && *edited != lines) {
            loot = lootOf(*edited);
        }
        return true;
    };
    hooks.displayItem = [&sink](world::LoadedWorld& world, const VaultSystem::Pos& pos,
                                std::optional<ItemStack>& item) {
        if (!sink.listening(&types::blockVaultDisplayItem)) {
            return true;
        }
        const PluginEvent* event = sink.ask(&types::blockVaultDisplayItem, [&](PluginEvent& filled) {
            hookEvents::position(filled, world.name(), pos[0], pos[1], pos[2]);
            filled.set("item", item.has_value() ? item->item.toString() : std::string{"minecraft:air"});
            filled.set("count", static_cast<std::int64_t>(item.has_value() ? item->count : 0));
        });
        if (event == nullptr) {
            return true;
        }
        if (event->cancelled()) {
            return false;
        }
        const auto* name = std::get_if<std::string>(&event->get("item"));
        const auto* count = std::get_if<std::int64_t>(&event->get("count"));
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            name == nullptr ? std::unexpected{wire::common::IdentifierError{}} : wire::common::Identifier::parse(*name);
        if (!id.has_value() || count == nullptr || *count <= 0 || id->path() == "air") {
            if (name != nullptr && count != nullptr) {
                item.reset();
            }
            return true;
        }
        if (!item.has_value() || item->item != *id) {
            item = ItemStack{.item = *id, .count = 1};
        }
        item->count = static_cast<std::int32_t>(*count);
        return true;
    };
    hooks.changeState = [&sink](world::LoadedWorld& world, const VaultSystem::Pos& pos,
                                const player::PlayerSession* who, world::vaults::State from, world::vaults::State to) {
        return sink.allowed(&types::blockVaultChangeState, [&](PluginEvent& event) {
            hookEvents::position(event, world.name(), pos[0], pos[1], pos[2]);
            event.set("player_name", who == nullptr ? std::string{} : who->name());
            event.set("player_uuid", who == nullptr ? std::string{} : who->uuid().toString());
            event.set("from", std::string{world::vaults::nameOf(from)});
            event.set("to", std::string{world::vaults::nameOf(to)});
        });
    };
    return hooks;
}

} // namespace basalt::server::runtime::plugins
