#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/plugins/EventHooks.hpp>
#include <basalt/server/runtime/plugins/PluginItemConversion.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <string>

namespace basalt::server::runtime::plugins::eventHooks {
namespace {

using server::plugins::PluginEvent;
using server::plugins::pluginEventTypes::inventoryChanged;
using server::plugins::pluginEventTypes::inventoryClick;
using server::plugins::pluginEventTypes::inventoryClose;
using server::plugins::pluginEventTypes::inventoryDrag;
using server::plugins::pluginEventTypes::inventoryEnchant;
using server::plugins::pluginEventTypes::inventoryEnchantPrepare;
using server::plugins::pluginEventTypes::inventoryOpen;
using server::plugins::pluginEventTypes::inventoryPrepare;
using server::plugins::pluginEventTypes::inventoryTake;
using server::plugins::pluginEventTypes::playerPickItem;

void position(PluginEvent& event, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    event.set("x", static_cast<std::int64_t>(x));
    event.set("y", static_cast<std::int64_t>(y));
    event.set("z", static_cast<std::int64_t>(z));
}

} // namespace

ContainerMenuController::Hooks containerMenu(ClientSession& owner, const EventSink& sink) {
    ContainerMenuController::Hooks hooks;
    hooks.openAllowed = [&owner, &sink](std::string_view kind,
                                        const std::optional<wire::protocol::play::BlockPos>& pos) {
        const player::PlayerSession* who = owner.playerSession();
        if (who == nullptr) {
            return true;
        }
        return sink.allowed(&inventoryOpen, [&](PluginEvent& event) {
            EventSink::player(event, *who);
            event.set("kind", std::string{kind});
            position(event, pos.has_value() ? pos->x : 0, pos.has_value() ? pos->y : 0, pos.has_value() ? pos->z : 0);
        });
    };
    hooks.clickAllowed = [&owner, &sink](std::int32_t slot, std::int32_t button, std::string_view mode,
                                         const std::optional<std::string>& item) {
        const player::PlayerSession* who = owner.playerSession();
        if (who == nullptr) {
            return true;
        }
        return sink.allowed(&inventoryClick, [&](PluginEvent& event) {
            EventSink::player(event, *who);
            event.set("slot", static_cast<std::int64_t>(slot));
            event.set("button", static_cast<std::int64_t>(button));
            event.set("mode", std::string{mode});
            event.set("item", item.value_or(std::string{}));
        });
    };
    hooks.pickAllowed = [&sink](const player::PlayerSession& who, std::int32_t x, std::int32_t y, std::int32_t z,
                                const std::string& item) {
        return sink.allowed(&playerPickItem, [&](PluginEvent& event) {
            EventSink::player(event, who);
            position(event, x, y, z);
            event.set("item", item);
        });
    };
    hooks.closed = [&owner, &sink](std::string_view kind, std::string_view reason) {
        if (const player::PlayerSession* who = owner.playerSession(); who != nullptr) {
            sink.notify(&inventoryClose, [&](PluginEvent& event) {
                EventSink::player(event, *who);
                event.set("kind", std::string{kind});
                event.set("reason", std::string{reason});
            });
        }
    };
    hooks.dragAllowed = [&owner, &sink](std::string_view type, std::string_view slots) {
        const player::PlayerSession* who = owner.playerSession();
        if (who == nullptr) {
            return true;
        }
        return sink.allowed(&inventoryDrag, [&](PluginEvent& event) {
            EventSink::player(event, *who);
            event.set("type", std::string{type});
            event.set("slots", std::string{slots});
        });
    };
    hooks.prepared = [&owner, &sink](std::string_view kind, std::string_view recipe, std::string_view name,
                                     std::optional<storage::StoredItemStack>& result, std::int32_t& cost) {
        const player::PlayerSession* who = owner.playerSession();
        if (who == nullptr) {
            return;
        }
        const server::plugins::PluginEvent* answered = sink.ask(&inventoryPrepare, [&](PluginEvent& event) {
            EventSink::player(event, *who);
            event.set("kind", std::string{kind});
            event.set("recipe", std::string{recipe});
            event.set("result", result.has_value() ? snbtOf(*result) : std::string{});
            event.set("name", std::string{name});
            event.set("cost", static_cast<std::int64_t>(cost));
        });
        if (answered == nullptr) {
            return;
        }
        if (const auto* text = std::get_if<std::string>(&answered->get("result")); text != nullptr) {
            result = stackFromSnbt(*text);
        }
        if (const auto* value = std::get_if<std::int64_t>(&answered->get("cost")); value != nullptr) {
            cost = static_cast<std::int32_t>(*value);
        }
    };
    hooks.takeAllowed = [&owner, &sink](std::string_view kind, std::string_view recipe,
                                        const storage::StoredItemStack& result, bool shift) {
        const player::PlayerSession* who = owner.playerSession();
        if (who == nullptr) {
            return true;
        }
        return sink.allowed(&inventoryTake, [&](PluginEvent& event) {
            EventSink::player(event, *who);
            event.set("kind", std::string{kind});
            event.set("recipe", std::string{recipe});
            event.set("result", snbtOf(result));
            event.set("shift", shift);
        });
    };
    hooks.enchantPrepared = [&owner, &sink](const storage::StoredItemStack& item, std::int32_t shelves,
                                            std::array<world::enchanting::Row, 3>& rows) {
        const player::PlayerSession* who = owner.playerSession();
        if (who == nullptr) {
            return true;
        }
        const auto describe = [&rows] {
            std::string text;
            for (const world::enchanting::Row& row : rows) {
                if (!text.empty()) {
                    text += ';';
                }
                if (!row.offers.empty()) {
                    text += row.offers.front().enchantment.toString() + '|' + std::to_string(row.offers.front().level) +
                            '|' + std::to_string(row.cost);
                }
            }
            return text;
        };
        const server::plugins::PluginEvent* answered = sink.ask(&inventoryEnchantPrepare, [&](PluginEvent& event) {
            EventSink::player(event, *who);
            if (const auto& at = owner.menus().openBlock; at.has_value()) {
                position(event, at->x, at->y, at->z);
            }
            event.set("item", snbtOf(item));
            event.set("bookshelves", static_cast<std::int64_t>(shelves));
            event.set("offers", describe());
        });
        if (answered == nullptr) {
            return true;
        }
        if (answered->cancelled()) {
            return false;
        }
        if (const auto* text = std::get_if<std::string>(&answered->get("offers")); text != nullptr) {
            std::size_t row = 0;
            for (std::size_t from = 0; row < rows.size(); ++row) {
                const std::size_t end = std::min(text->find(';', from), text->size());
                const std::string entry = text->substr(from, end - from);
                from = end + 1;
                const std::size_t first = entry.find('|');
                const std::size_t second = entry.find('|', first == std::string::npos ? 0 : first + 1);
                if (first == std::string::npos || second == std::string::npos) {
                    rows[row] = {};
                } else {
                    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
                        wire::common::Identifier::parse(entry.substr(0, first));
                    if (id.has_value()) {
                        rows[row].offers = {world::enchanting::Offer{
                            .enchantment = *id,
                            .level = std::atoi(entry.substr(first + 1, second - first - 1).c_str())}};
                        rows[row].clue = rows[row].offers.front();
                        rows[row].cost = std::atoi(entry.substr(second + 1).c_str());
                    }
                }
                if (end >= text->size()) {
                    ++row;
                    break;
                }
            }
        }
        return true;
    };
    hooks.enchantAllowed = [&owner, &sink](const storage::StoredItemStack& item, std::int32_t button,
                                           std::int32_t& cost, std::vector<world::enchanting::Offer>& offers) {
        const player::PlayerSession* who = owner.playerSession();
        if (who == nullptr) {
            return true;
        }
        std::string text;
        for (const world::enchanting::Offer& offer : offers) {
            text += (text.empty() ? "" : ",") + offer.enchantment.toString() + ':' + std::to_string(offer.level);
        }
        const server::plugins::PluginEvent* answered = sink.ask(&inventoryEnchant, [&](PluginEvent& event) {
            EventSink::player(event, *who);
            if (const auto& at = owner.menus().openBlock; at.has_value()) {
                position(event, at->x, at->y, at->z);
            }
            event.set("item", snbtOf(item));
            event.set("button", static_cast<std::int64_t>(button));
            event.set("cost", static_cast<std::int64_t>(cost));
            event.set("enchants", text);
        });
        if (answered == nullptr) {
            return true;
        }
        if (answered->cancelled()) {
            return false;
        }
        if (const auto* value = std::get_if<std::int64_t>(&answered->get("cost")); value != nullptr) {
            cost = static_cast<std::int32_t>(*value);
        }
        if (const auto* chosen = std::get_if<std::string>(&answered->get("enchants")); chosen != nullptr) {
            offers.clear();
            for (std::size_t from = 0; from < chosen->size();) {
                const std::size_t end = std::min(chosen->find(',', from), chosen->size());
                const std::string entry = chosen->substr(from, end - from);
                from = end + 1;
                const std::size_t colon = entry.rfind(':');
                const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
                    wire::common::Identifier::parse(entry.substr(0, colon));
                if (colon != std::string::npos && id.has_value()) {
                    offers.push_back(
                        world::enchanting::Offer{.enchantment = *id, .level = std::atoi(entry.c_str() + colon + 1)});
                }
            }
        }
        return !offers.empty();
    };
    hooks.changed = [&owner, &sink]() {
        if (const player::PlayerSession* who = owner.playerSession(); who != nullptr) {
            sink.notify(&inventoryChanged, [&](PluginEvent& event) { EventSink::player(event, *who); });
        }
    };
    return hooks;
}

} // namespace basalt::server::runtime::plugins::eventHooks
