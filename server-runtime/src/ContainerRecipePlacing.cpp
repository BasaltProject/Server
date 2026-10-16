#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/RecipeBook.hpp>
#include <basalt/server/world/crafting/StackedContents.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime {
namespace {

using player::PlayerInventory;
using storage::StoredItemStack;
using Grid = std::optional<StoredItemStack>;
namespace crafting = server::world::crafting;

constexpr std::int32_t kInventoryItems = 36;

/// parity: `Inventory.isUsableForCrafting`.
[[nodiscard]] bool usableForCrafting(const StoredItemStack& stack) {
    const wire::nbt::NbtCompound* enchantments = stack.components.getCompound("minecraft:enchantments");
    return !(item::itemWear::isDamageable(stack) && item::itemWear::damage(stack) > 0) &&
           (enchantments == nullptr || enchantments->empty()) && !stack.components.contains("minecraft:custom_name");
}

[[nodiscard]] bool sameItemSameComponents(const StoredItemStack& left, const StoredItemStack& right) {
    return left.id == right.id && left.components == right.components;
}

/// parity: `PlacementInfo`, the recipe's non-empty ingredients and which one each grid cell takes.
struct Placement {
    std::vector<const crafting::Ingredient*> ingredients;
    std::vector<std::int32_t> slotToIngredient;
    std::int32_t recipeWidth = 0;
    std::int32_t recipeHeight = 0;

    [[nodiscard]] bool impossible() const { return slotToIngredient.empty(); }
};

[[nodiscard]] Placement placementOf(const crafting::Recipe& recipe, std::int32_t gridWidth, std::int32_t gridHeight) {
    Placement placement{.recipeWidth = gridWidth, .recipeHeight = gridHeight};
    if (recipe.kind == crafting::RecipeKind::Shaped) {
        placement.recipeWidth = recipe.width;
        placement.recipeHeight = recipe.height;
        for (const std::optional<crafting::Ingredient>& cell : recipe.shape) {
            if (!cell.has_value()) {
                placement.slotToIngredient.push_back(-1);
                continue;
            }
            if (cell->items.empty()) {
                return {};
            }
            placement.slotToIngredient.push_back(static_cast<std::int32_t>(placement.ingredients.size()));
            placement.ingredients.push_back(&*cell);
        }
        return placement;
    }
    for (const crafting::Ingredient& ingredient : recipe.ingredients) {
        if (ingredient.items.empty()) {
            return {};
        }
        placement.slotToIngredient.push_back(static_cast<std::int32_t>(placement.ingredients.size()));
        placement.ingredients.push_back(&ingredient);
    }
    return placement;
}

/// parity: `PlaceRecipeHelper.placeRecipe`.
template <typename Output>
void walkPlacement(std::int32_t gridWidth, std::int32_t gridHeight, std::int32_t recipeWidth, std::int32_t recipeHeight,
                   const std::vector<std::int32_t>& entries, Output&& output) {
    std::size_t next = 0;
    std::int32_t gridIndex = 0;
    for (std::int32_t y = 0; y < gridHeight; ++y) {
        bool center = static_cast<float>(recipeHeight) < static_cast<float>(gridHeight) / 2.0F;
        std::int32_t start = static_cast<std::int32_t>(
            std::floor((static_cast<float>(gridHeight) / 2.0F) - (static_cast<float>(recipeHeight) / 2.0F)));
        if (center && start > y) {
            gridIndex += gridWidth;
            ++y;
        }
        for (std::int32_t x = 0; x < gridWidth; ++x) {
            if (next >= entries.size()) {
                return;
            }
            center = static_cast<float>(recipeWidth) < static_cast<float>(gridWidth) / 2.0F;
            start = static_cast<std::int32_t>(
                std::floor((static_cast<float>(gridWidth) / 2.0F) - (static_cast<float>(recipeWidth) / 2.0F)));
            std::int32_t totalWidth = recipeWidth;
            bool add = x < recipeWidth;
            if (center) {
                totalWidth = start + recipeWidth;
                add = start <= x && x < start + recipeWidth;
            }
            if (add) {
                output(entries[next++], gridIndex);
            } else if (totalWidth == x) {
                gridIndex += gridWidth - x;
                break;
            }
            ++gridIndex;
        }
    }
}

} // namespace

// parity: `ServerPlaceRecipe`; `StackedContents` picks in insertion order where vanilla uses identity hashes.
void ContainerMenuController::placeRecipe(const wire::protocol::play::PlaceRecipePacket& packet) {
    player::PlayerSession* session = owner_->playerSession();
    world::RealRecipes* recipes = owner_->options().recipes;
    world::PlayerProgress* progress = owner_->options().progress;
    const std::int32_t gridSize = openMenu.gridSize();
    if (session == nullptr || recipes == nullptr || progress == nullptr || gridSize <= 0 ||
        session->gamemode == wire::protocol::play::Gamemode::Spectator ||
        packet.containerId != openMenu.containerId() || packet.recipeId < 0 ||
        packet.recipeId >= static_cast<std::int32_t>(recipes->ids().size())) {
        return;
    }
    const wire::common::Identifier id = *std::next(recipes->ids().begin(), packet.recipeId);
    const crafting::Recipe* recipe = recipes->recipeFor(id);
    const std::optional<wire::protocol::play::RecipeBookEntry> entry = world::recipeBook::entryFor(*recipes, id);
    if (recipe == nullptr || !entry.has_value() || !progress->knownRecipes(session->uuid().toString()).contains(id)) {
        return;
    }
    const Placement placement = placementOf(*recipe, openMenu.gridWidth(), openMenu.gridHeight());
    if (placement.impossible()) {
        return;
    }

    PlayerInventory& inventory = session->inventory;
    const auto cellAt = [this](std::int32_t index) -> Grid& { return craftingGrid_[static_cast<std::size_t>(index)]; };

    const auto freeSlots = [&inventory] {
        std::int32_t free = 0;
        for (std::int32_t slot = 0; slot < kInventoryItems; ++slot) {
            free += inventory.at(slot).has_value() ? 0 : 1;
        }
        return free;
    };
    const bool creative = session->gamemode == wire::protocol::play::Gamemode::Creative;
    if (!creative) {
        // parity: `ServerPlaceRecipe.testClearGrid`.
        std::vector<StoredItemStack> pending;
        const std::int32_t free = freeSlots();
        for (std::int32_t index = 0; index < gridSize; ++index) {
            if (!cellAt(index).has_value()) {
                continue;
            }
            StoredItemStack stack = *cellAt(index);
            const bool roomed = inventory.slotWithRoomFor(stack).has_value();
            if (!roomed && static_cast<std::int32_t>(pending.size()) <= free) {
                for (StoredItemStack& other : pending) {
                    const std::int32_t limit = PlayerInventory::maxStackIn(other);
                    if (other.id == stack.id && other.count != limit && other.count + stack.count <= limit) {
                        other.count += stack.count;
                        stack.count = 0;
                        break;
                    }
                }
                if (stack.count > 0) {
                    if (static_cast<std::int32_t>(pending.size()) >= free) {
                        return;
                    }
                    pending.push_back(std::move(stack));
                }
            } else if (!roomed) {
                return;
            }
        }
    }

    crafting::StackedContents available;
    const auto account = [&available](const StoredItemStack& stack) {
        if (usableForCrafting(stack)) {
            available.account(stack.id, std::min(PlayerInventory::maxStackIn(stack), stack.count));
        }
    };
    for (std::int32_t slot = 0; slot < kInventoryItems; ++slot) {
        if (inventory.at(slot).has_value()) {
            account(*inventory.at(slot));
        }
    }
    for (std::int32_t index = 0; index < gridSize; ++index) {
        if (cellAt(index).has_value()) {
            account(*cellAt(index));
        }
    }

    const auto clearGrid = [&] {
        for (std::int32_t index = 0; index < gridSize; ++index) {
            if (cellAt(index).has_value()) {
                static_cast<void>(inventory.add(*cellAt(index)));
                cellAt(index).reset();
            }
        }
        craftingResult_.reset();
    };
    const auto finish = [&] {
        recomputeCraftingResult();
        sendInventory();
    };

    const std::span<const crafting::Ingredient* const> ingredients{placement.ingredients};
    if (!available.tryPick(ingredients, 1)) {
        clearGrid();
        finish();
        owner_->sendPlayPacket(wire::protocol::play::PlaceGhostRecipePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::PlaceGhostRecipePacket{.containerId = openMenu.containerId(),
                                                         .display = entry->display}
                .write(buffer);
        });
        return;
    }

    std::vector<std::optional<server::world::loot::ItemStack>> cells;
    for (std::int32_t index = 0; index < gridSize; ++index) {
        const Grid& cell = cellAt(index);
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> cellId =
            cell.has_value() ? wire::common::Identifier::parse(cell->id)
                             : std::unexpected{wire::common::IdentifierError{}};
        cells.push_back(cellId.has_value() ? std::optional{server::world::loot::ItemStack{
                                                 .item = *cellId, .count = cell->count, .components = cell->components}}
                                           : std::nullopt);
    }
    const bool matchesPlaced =
        recipe->matches(crafting::CraftingInput::of(openMenu.gridWidth(), openMenu.gridHeight(), std::move(cells)));
    const std::int32_t biggest = available.tryPickAll(ingredients, std::numeric_limits<std::int32_t>::max());
    if (matchesPlaced) {
        for (std::int32_t index = 0; index < gridSize; ++index) {
            if (cellAt(index).has_value() &&
                std::min(biggest, PlayerInventory::maxStackIn(*cellAt(index))) < cellAt(index)->count + 1) {
                return;
            }
        }
    }

    std::int32_t amount = 1;
    if (packet.useMaxItems) {
        amount = biggest;
    } else if (matchesPlaced) {
        std::int32_t smallest = std::numeric_limits<std::int32_t>::max();
        for (std::int32_t index = 0; index < gridSize; ++index) {
            if (cellAt(index).has_value()) {
                smallest = std::min(smallest, cellAt(index)->count);
            }
        }
        amount = smallest == std::numeric_limits<std::int32_t>::max() ? smallest : smallest + 1;
    }

    std::vector<std::string> used;
    if (!available.tryPick(ingredients, amount, &used)) {
        return;
    }
    const auto clamp = [&used](std::int32_t value) {
        for (const std::string& item : used) {
            const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsed =
                wire::common::Identifier::parse(item);
            value = std::min(value, parsed.has_value() ? server::registry::itemStacking::maxStackSize(*parsed) : 1);
        }
        return value;
    };
    const std::int32_t adjusted = clamp(amount);
    if (adjusted != amount) {
        used.clear();
        if (!available.tryPick(ingredients, adjusted, &used)) {
            return;
        }
    }

    clearGrid();
    walkPlacement(openMenu.gridWidth(), openMenu.gridHeight(), placement.recipeWidth, placement.recipeHeight,
                  placement.slotToIngredient, [&](std::int32_t ingredientIndex, std::int32_t gridIndex) {
                      if (ingredientIndex == -1 || gridIndex >= gridSize) {
                          return;
                      }
                      const std::string& item = used[static_cast<std::size_t>(ingredientIndex)];
                      Grid& target = cellAt(gridIndex);
                      for (std::int32_t remaining = adjusted; remaining > 0;) {
                          // parity: `Inventory.findSlotMatchingCraftingIngredient`.
                          std::optional<std::int32_t> found;
                          for (std::int32_t slot = 0; slot < kInventoryItems; ++slot) {
                              const Grid& held = inventory.at(slot);
                              if (held.has_value() && held->id == item && usableForCrafting(*held) &&
                                  (!target.has_value() || sameItemSameComponents(*target, *held))) {
                                  found = slot;
                                  break;
                              }
                          }
                          if (!found.has_value()) {
                              return;
                          }
                          StoredItemStack taken = *inventory.at(*found);
                          taken.count = std::min(remaining, taken.count);
                          StoredItemStack left = *inventory.at(*found);
                          left.count -= taken.count;
                          inventory.set(*found, left.count > 0 ? std::optional{left} : std::nullopt);
                          remaining -= taken.count;
                          if (target.has_value()) {
                              target->count += taken.count;
                          } else {
                              target = taken;
                          }
                      }
                  });
    finish();
}

} // namespace basalt::server::runtime
