#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/world/RecipeBook.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <iterator>
#include <optional>
#include <set>
#include <string>

namespace basalt::server::runtime::world::recipeBook {
namespace {

namespace reg = server::registry;
namespace crafting = server::world::crafting;

using wire::common::Identifier;
using wire::protocol::play::RecipeBookEntry;
using wire::protocol::play::RecipeDisplay;
using wire::protocol::play::SlotDisplay;

/// Protocol ids, dropping anything this build's item registry does not know.
[[nodiscard]] std::vector<std::int32_t> itemIds(const std::set<std::string, std::less<>>& items) {
    std::vector<std::int32_t> ids;
    for (const std::string& name : items) {
        const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(name);
        if (!parsed.has_value()) {
            continue;
        }
        if (const reg::Item* item = reg::items::registry().byName(*parsed); item != nullptr) {
            ids.push_back(item->protocolId);
        }
    }
    return ids;
}

/// One slot: a single item, or a choice between the items an ingredient accepts.
[[nodiscard]] std::optional<SlotDisplay> slotFor(const std::set<std::string, std::less<>>& items) {
    const std::vector<std::int32_t> ids = itemIds(items);
    if (ids.empty()) {
        return std::nullopt;
    }
    if (ids.size() == 1) {
        return SlotDisplay{SlotDisplay::Item{.itemId = ids.front()}};
    }
    std::vector<SlotDisplay> options;
    options.reserve(ids.size());
    for (const std::int32_t id : ids) {
        options.emplace_back(SlotDisplay::Item{.itemId = id});
    }
    return SlotDisplay{SlotDisplay::Composite{.options = std::move(options)}};
}

/// Where this recipe sits in the numbered list the client indexes by.
[[nodiscard]] std::optional<std::int32_t> numberOf(const RealRecipes& recipes, const Identifier& id) {
    const auto found = recipes.ids().find(id);
    if (found == recipes.ids().end()) {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(std::distance(recipes.ids().begin(), found));
}

} // namespace

std::optional<RecipeBookEntry> entryFor(const RealRecipes& recipes, const Identifier& id) {
    const crafting::Recipe* recipe = recipes.recipeFor(id);
    if (recipe == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> number = numberOf(recipes, id);
    if (!number.has_value()) {
        return std::nullopt;
    }
    const std::optional<SlotDisplay> result = slotFor({std::string{recipe->result.item.toString()}});
    if (!result.has_value()) {
        return std::nullopt;
    }
    const std::optional<SlotDisplay> station = slotFor({std::string{"minecraft:crafting_table"}});

    RecipeDisplay display;
    display.result = *result;
    display.craftingStation = station.value_or(SlotDisplay{SlotDisplay::Empty{}});
    std::vector<std::vector<std::int32_t>> requirements;

    // The book's furnace tabs come with the furnace menu, the stonecutter and the smithing table
    // draw their own lists, and a special has no fixed shape to draw at all.
    if (recipe->kind == crafting::RecipeKind::Shaped) {
        display.grid = RecipeDisplay::Grid{.width = recipe->width, .height = recipe->height};
        for (const std::optional<crafting::Ingredient>& cell : recipe->shape) {
            const std::optional<SlotDisplay> slot =
                cell.has_value() ? slotFor(cell->items) : std::optional<SlotDisplay>{};
            display.ingredients.push_back(slot.value_or(SlotDisplay{SlotDisplay::Empty{}}));
            if (cell.has_value()) {
                std::vector<std::int32_t> ids = itemIds(cell->items);
                if (!ids.empty()) {
                    requirements.push_back(std::move(ids));
                }
            }
        }
    } else if (recipe->kind == crafting::RecipeKind::Shapeless) {
        for (const crafting::Ingredient& ingredient : recipe->ingredients) {
            display.ingredients.push_back(slotFor(ingredient.items).value_or(SlotDisplay{SlotDisplay::Empty{}}));
            std::vector<std::int32_t> ids = itemIds(ingredient.items);
            if (!ids.empty()) {
                requirements.push_back(std::move(ids));
            }
        }
    } else {
        return std::nullopt;
    }

    return RecipeBookEntry{.recipeId = *number,
                           .display = std::move(display),
                           .category = RecipeBookEntry::kCraftingMiscCategory,
                           .craftingRequirements = std::move(requirements)};
}

void add(const player::PlayerSession& target, const RealRecipes& recipes, const std::vector<Identifier>& ids) {
    std::vector<RecipeBookEntry> entries;
    for (const Identifier& id : ids) {
        std::optional<RecipeBookEntry> entry = entryFor(recipes, id);
        if (entry.has_value()) {
            entries.push_back(std::move(*entry));
        }
    }
    if (entries.empty() || !target.connection().sendPlayPacket) {
        return;
    }
    const wire::protocol::play::RecipeBookAddPacket packet{.entries = std::move(entries), .replace = false};
    target.connection().sendPlayPacket(wire::protocol::play::RecipeBookAddPacket::kId,
                                       [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
}

void remove(const player::PlayerSession& target, const RealRecipes& recipes, const std::vector<Identifier>& ids) {
    std::vector<std::int32_t> numbered;
    for (const Identifier& id : ids) {
        const std::optional<std::int32_t> number = numberOf(recipes, id);
        if (number.has_value()) {
            numbered.push_back(*number);
        }
    }
    if (numbered.empty() || !target.connection().sendPlayPacket) {
        return;
    }
    const wire::protocol::play::RecipeBookRemovePacket packet{.recipeIds = std::move(numbered)};
    target.connection().sendPlayPacket(wire::protocol::play::RecipeBookRemovePacket::kId,
                                       [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
}

} // namespace basalt::server::runtime::world::recipeBook
