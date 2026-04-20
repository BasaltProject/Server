#include <basalt/server/runtime/bedrock/BedrockStackRequests.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::bedrock {
namespace {

using player::PlayerInventory;
using wire::bedrock::BedrockContainerSlot;
using wire::bedrock::BedrockResponseContainer;
using wire::bedrock::BedrockResponseSlot;
using wire::bedrock::BedrockStackAction;
using wire::bedrock::BedrockStackActionType;
using wire::bedrock::BedrockStackSlot;

// A table's nine cells run from 32 and the player's own four from 28 under one window number.
constexpr std::int32_t kFirstTableSlot = 32;
constexpr std::int32_t kFirstCarriedSlot = 28;
constexpr std::int32_t kCarriedCells = 4;

// This server has one slot list per player, one dragged stack, a table's grid and the open block.
struct Place {
    enum class Kind : std::uint8_t { None, Storage, Cursor, Block, Grid, Created };

    Kind kind = Kind::None;
    std::int32_t slot = 0;
};

// Bedrock numbers the 36 carried slots hotbar first; this server puts it last.
[[nodiscard]] std::int32_t carriedSlotOf(const std::int32_t slot) {
    return slot < PlayerInventory::kHotbarSize ? PlayerInventory::kHotbarStart + slot
                                               : slot - PlayerInventory::kHotbarSize;
}

[[nodiscard]] Place placeOf(const BedrockStackSlot& named, const BedrockWindows& windows) {
    if (named.container < 0 || named.container > 0xFF) {
        return {};
    }
    if (windows.open.has_value()) {
        if (const std::optional<std::int32_t> slot = blockSlotOf(*windows.open, named.container, named.slot);
            slot.has_value()) {
            return {.kind = Place::Kind::Block, .slot = *slot};
        }
    }
    const auto container = static_cast<BedrockContainerSlot>(named.container);
    if (container == BedrockContainerSlot::CraftingInput) {
        // Both grids count left to right then down; the player's four land on the first four cells.
        if (named.slot >= kFirstCarriedSlot && named.slot < kFirstCarriedSlot + kCarriedCells) {
            return {.kind = Place::Kind::Grid, .slot = named.slot - kFirstCarriedSlot};
        }
        const std::int32_t cell = named.slot - kFirstTableSlot;
        if (cell < 0 || cell >= static_cast<std::int32_t>(kGridCells)) {
            return {};
        }
        return {.kind = Place::Kind::Grid, .slot = cell};
    }
    switch (container) {
    case BedrockContainerSlot::HotbarAndInventory:
        if (named.slot < 0 || named.slot >= PlayerInventory::kMainSlots) {
            return {};
        }
        return {.kind = Place::Kind::Storage, .slot = carriedSlotOf(named.slot)};
    case BedrockContainerSlot::Hotbar:
        if (named.slot < 0 || named.slot >= PlayerInventory::kHotbarSize) {
            return {};
        }
        return {.kind = Place::Kind::Storage, .slot = PlayerInventory::kHotbarStart + named.slot};
    case BedrockContainerSlot::Inventory:
        if (named.slot < 0 || named.slot >= PlayerInventory::kHotbarStart) {
            return {};
        }
        return {.kind = Place::Kind::Storage, .slot = named.slot};
    case BedrockContainerSlot::Armour:
        if (named.slot < 0 || named.slot >= PlayerInventory::kArmourSlots) {
            return {};
        }
        return {.kind = Place::Kind::Storage, .slot = PlayerInventory::kArmourStart + named.slot};
    case BedrockContainerSlot::Offhand:
        // That edition numbers the one off-hand slot from one, and sends zero for it as well.
        if (named.slot > 1) {
            return {};
        }
        return {.kind = Place::Kind::Storage, .slot = PlayerInventory::kOffhandSlot};
    case BedrockContainerSlot::EnchantingInput:
        // Screen slots of these tables live where a grid's first cells do: only one screen is ever open.
        return {.kind = Place::Kind::Grid, .slot = 0};
    case BedrockContainerSlot::EnchantingMaterial:
        return {.kind = Place::Kind::Grid, .slot = 1};
    case BedrockContainerSlot::SmithingTemplate:
        return {.kind = Place::Kind::Grid, .slot = 0};
    case BedrockContainerSlot::SmithingInput:
        return {.kind = Place::Kind::Grid, .slot = 1};
    case BedrockContainerSlot::SmithingMaterial:
        return {.kind = Place::Kind::Grid, .slot = 2};
    case BedrockContainerSlot::GrindstoneInput:
        return {.kind = Place::Kind::Grid, .slot = 0};
    case BedrockContainerSlot::GrindstoneAdditional:
        return {.kind = Place::Kind::Grid, .slot = 1};
    case BedrockContainerSlot::AnvilInput:
        return {.kind = Place::Kind::Grid, .slot = 0};
    case BedrockContainerSlot::AnvilMaterial:
        return {.kind = Place::Kind::Grid, .slot = 1};
    case BedrockContainerSlot::LoomInput:
        return {.kind = Place::Kind::Grid, .slot = 0};
    case BedrockContainerSlot::LoomDye:
        return {.kind = Place::Kind::Grid, .slot = 1};
    case BedrockContainerSlot::LoomMaterial:
        return {.kind = Place::Kind::Grid, .slot = 2};
    case BedrockContainerSlot::CartographyInput:
        return {.kind = Place::Kind::Grid, .slot = 0};
    case BedrockContainerSlot::CartographyAdditional:
        return {.kind = Place::Kind::Grid, .slot = 1};
    case BedrockContainerSlot::BeaconPayment:
        return {.kind = Place::Kind::Grid, .slot = 0};
    case BedrockContainerSlot::StonecutterInput:
        return {.kind = Place::Kind::Grid, .slot = 0};
    case BedrockContainerSlot::TradeIngredientA:
        // A trade's payment is laid in the first two cells: nothing else is open behind a merchant's book.
        return {.kind = Place::Kind::Grid, .slot = 0};
    case BedrockContainerSlot::TradeIngredientB:
        return {.kind = Place::Kind::Grid, .slot = 1};
    case BedrockContainerSlot::Cursor:
        return {.kind = Place::Kind::Cursor};
    case BedrockContainerSlot::CreatedOutput:
    case BedrockContainerSlot::CraftingOutput:
    case BedrockContainerSlot::StonecutterResult:
    case BedrockContainerSlot::TradeResult:
    case BedrockContainerSlot::AnvilResult:
    case BedrockContainerSlot::GrindstoneResult:
    case BedrockContainerSlot::SmithingResult:
    case BedrockContainerSlot::LoomResult:
    case BedrockContainerSlot::CartographyResult:
        return {.kind = Place::Kind::Created};
    default:
        return {};
    }
}

// Writes are staged until every line works: requests are all or nothing, or the two sides disagree about a chest.
struct Reach {
    player::PlayerSession& session;
    BedrockWindows& windows;
    world::LoadedWorld* world = nullptr;
    std::map<std::int32_t, std::optional<storage::StoredItemStack>> inventory;
    std::map<std::int32_t, std::optional<storage::StoredItemStack>> block;
    std::map<std::int32_t, std::optional<storage::StoredItemStack>> grid;
    std::optional<std::optional<storage::StoredItemStack>> cursor;
    std::vector<storage::StoredItemStack> dropped;
    // Exists only between the craft line and the one that takes it, which is how this edition hands a result over.
    std::optional<storage::StoredItemStack> created;
    // The offer is not counted as used until every line of the request has worked.
    std::optional<std::int32_t> trade;
    std::int32_t trades = 0;
    std::optional<std::int32_t> anvilCost;
    std::optional<BeaconChoice> beacon;
    std::optional<std::int32_t> grindstoneWorth;
    std::optional<std::int32_t> enchantCost;
    // Its consume lines have to take exactly one of each.
    std::optional<std::array<std::int32_t, 2>> cartographyBefore;
};

[[nodiscard]] std::optional<storage::StoredItemStack> stackAt(const Reach& reach, const Place& place) {
    switch (place.kind) {
    case Place::Kind::Storage: {
        const auto staged = reach.inventory.find(place.slot);
        return staged == reach.inventory.end() ? reach.session.inventory.at(place.slot) : staged->second;
    }
    case Place::Kind::Cursor:
        return reach.cursor.has_value() ? *reach.cursor : reach.windows.cursor;
    case Place::Kind::Created:
        return reach.created;
    case Place::Kind::Grid: {
        const auto staged = reach.grid.find(place.slot);
        return staged == reach.grid.end() ? reach.windows.grid[static_cast<std::size_t>(place.slot)] : staged->second;
    }
    case Place::Kind::Block: {
        const auto staged = reach.block.find(place.slot);
        if (staged != reach.block.end()) {
            return staged->second;
        }
        if (reach.world == nullptr || !reach.windows.open.has_value()) {
            return std::nullopt;
        }
        return readWindowSlot(*reach.world, *reach.windows.open, place.slot, &reach.session);
    }
    case Place::Kind::None:
        break;
    }
    return std::nullopt;
}

[[nodiscard]] bool writeStack(Reach& reach, const Place& place, std::optional<storage::StoredItemStack> stack) {
    switch (place.kind) {
    case Place::Kind::Storage:
        reach.inventory[place.slot] = std::move(stack);
        return true;
    case Place::Kind::Cursor:
        reach.cursor = std::move(stack);
        return true;
    case Place::Kind::Created:
        reach.created = std::move(stack);
        return true;
    case Place::Kind::Grid:
        reach.grid[place.slot] = std::move(stack);
        return true;
    case Place::Kind::Block:
        if (reach.world == nullptr || !reach.windows.open.has_value() || place.slot < 0 ||
            place.slot >= reach.windows.open->size) {
            return false;
        }
        reach.block[place.slot] = std::move(stack);
        return true;
    case Place::Kind::None:
        break;
    }
    return false;
}

// Runs only once the whole request has worked.
void commit(Reach& reach, const std::function<void(const storage::StoredItemStack&)>& drop) {
    for (const auto& [slot, stack] : reach.inventory) {
        reach.session.inventory.set(slot, stack);
    }
    for (const auto& [cell, stack] : reach.grid) {
        reach.windows.grid[static_cast<std::size_t>(cell)] = stack;
    }
    if (reach.cursor.has_value()) {
        reach.windows.cursor = *reach.cursor;
    }
    if (reach.world != nullptr && reach.windows.open.has_value()) {
        for (const auto& [slot, stack] : reach.block) {
            static_cast<void>(writeWindowSlot(*reach.world, *reach.windows.open, slot, stack, &reach.session));
        }
    }
    for (const storage::StoredItemStack& stack : reach.dropped) {
        drop(stack);
        player::statAwards::awardThrown(reach.session, stack.id, stack.count);
    }
}

[[nodiscard]] storage::StoredItemStack withCount(storage::StoredItemStack stack, const std::int32_t count) {
    stack.count = count;
    return stack;
}

[[nodiscard]] bool sameKind(const storage::StoredItemStack& a, const storage::StoredItemStack& b) {
    return a.id == b.id && a.components == b.components;
}

class Touched {
public:
    void add(const BedrockStackSlot& named) { slots_[named.container].insert(named.slot); }

    [[nodiscard]] std::vector<BedrockResponseContainer> answer(const Reach& reach) const {
        std::vector<BedrockResponseContainer> containers;
        containers.reserve(slots_.size());
        for (const auto& [container, slots] : slots_) {
            BedrockResponseContainer entry;
            entry.container = container;
            for (const std::int32_t slot : slots) {
                const Place place = placeOf({.container = container, .slot = slot}, reach.windows);
                const std::optional<storage::StoredItemStack> stack = stackAt(reach, place);
                entry.slots.push_back({.slot = slot, .count = stack.has_value() ? stack->count : 0});
            }
            containers.push_back(std::move(entry));
        }
        return containers;
    }

private:
    std::map<std::int32_t, std::set<std::int32_t>> slots_;
};

[[nodiscard]] bool moveStack(Reach& reach, const Place& from, const Place& to, const std::int32_t count) {
    if (from.kind == Place::Kind::None || to.kind == Place::Kind::None || count <= 0) {
        return false;
    }
    const std::optional<storage::StoredItemStack> source = stackAt(reach, from);
    if (!source.has_value() || source->count < count) {
        return false;
    }
    const std::optional<storage::StoredItemStack> target = stackAt(reach, to);
    if (target.has_value()) {
        if (!sameKind(*target, *source) || target->count + count > PlayerInventory::maxStackIn(*target)) {
            return false;
        }
        if (!writeStack(reach, to, withCount(*target, target->count + count))) {
            return false;
        }
    } else if (!writeStack(reach, to, withCount(*source, count))) {
        return false;
    }
    const std::int32_t left = source->count - count;
    return writeStack(reach, from, left <= 0 ? std::nullopt : std::optional{withCount(*source, left)});
}

[[nodiscard]] std::optional<storage::StoredItemStack> takeStack(Reach& reach, const Place& from,
                                                                const std::int32_t count) {
    if (from.kind == Place::Kind::None || count <= 0) {
        return std::nullopt;
    }
    const std::optional<storage::StoredItemStack> source = stackAt(reach, from);
    if (!source.has_value() || source->count < count) {
        return std::nullopt;
    }
    const std::int32_t left = source->count - count;
    const storage::StoredItemStack taken = withCount(*source, count);
    if (!writeStack(reach, from, left <= 0 ? std::nullopt : std::optional{withCount(*source, left)})) {
        return std::nullopt;
    }
    return taken;
}

// False leaves the staged writes to be thrown away with the rest of the request.
[[nodiscard]] bool payInto(Reach& reach, const std::int32_t cell, const server::world::loot::ItemStack& cost,
                           const std::int32_t times) {
    const std::string id = cost.item.toString();
    const std::int32_t wanted = cost.count * times;
    std::int32_t left = wanted;
    if (left <= 0) {
        return true;
    }
    for (std::int32_t slot = 0; slot < PlayerInventory::kMainSlots && left > 0; ++slot) {
        const Place place{.kind = Place::Kind::Storage, .slot = slot};
        const std::optional<storage::StoredItemStack> held = stackAt(reach, place);
        if (!held.has_value() || held->id != id) {
            continue;
        }
        const std::int32_t taken = std::min(left, held->count);
        if (!writeStack(reach, place,
                        held->count <= taken ? std::nullopt : std::optional{withCount(*held, held->count - taken)})) {
            return false;
        }
        left -= taken;
    }
    if (left > 0) {
        return false;
    }
    return writeStack(reach, {.kind = Place::Kind::Grid, .slot = cell},
                      storage::StoredItemStack{.id = id, .count = wanted});
}

/// Buys one of the open merchant's offers and stages what it hands over.
/// parity: `MerchantMenu.clickMenuButton`.
[[nodiscard]] bool takeTrade(Reach& reach, const std::int32_t index, const std::int32_t times) {
    if (!reach.windows.trades.has_value() || index < 0 || times <= 0 ||
        static_cast<std::size_t>(index) >= reach.windows.trades->offers.size()) {
        return false;
    }
    const world::trades::Offer& offer = reach.windows.trades->offers[static_cast<std::size_t>(index)];
    // parity: `MerchantOffer.isOutOfStock`.
    if (offer.uses + times > offer.maxUses) {
        return false;
    }
    const auto [costA, costB] = world::trades::currentCost(offer);
    if (!payInto(reach, 0, costA, times)) {
        return false;
    }
    if (costB.has_value() && !payInto(reach, 1, *costB, times)) {
        return false;
    }
    reach.created = storage::StoredItemStack{.id = offer.result.item.toString(), .count = offer.result.count * times};
    reach.trade = index;
    reach.trades = times;
    return true;
}

[[nodiscard]] bool forge(Reach& reach, const StackRequestContext& context) {
    if (!context.smithingResult) {
        return false;
    }
    std::array<Place, 3> cells{Place{.kind = Place::Kind::Grid, .slot = 0}, Place{.kind = Place::Kind::Grid, .slot = 1},
                               Place{.kind = Place::Kind::Grid, .slot = 2}};
    std::array<std::optional<storage::StoredItemStack>, 3> held{stackAt(reach, cells[0]), stackAt(reach, cells[1]),
                                                                stackAt(reach, cells[2])};
    const std::optional<storage::StoredItemStack> made =
        context.smithingResult(held[0].has_value() ? &*held[0] : nullptr, held[1].has_value() ? &*held[1] : nullptr,
                               held[2].has_value() ? &*held[2] : nullptr);
    if (!made.has_value()) {
        return false;
    }
    for (std::size_t at = 0; at < cells.size(); ++at) {
        if (!held[at].has_value()) {
            continue;
        }
        const std::int32_t left = held[at]->count - 1;
        if (!writeStack(reach, cells[at], left <= 0 ? std::nullopt : std::optional{withCount(*held[at], left)})) {
            return false;
        }
    }
    reach.created = *made;
    return true;
}

/// parity: `EnchantmentMenu.clickMenuButton`.
[[nodiscard]] bool takeEnchantment(Reach& reach, const std::int32_t netId, const StackRequestContext& context) {
    if (!reach.windows.enchanting.has_value()) {
        return false;
    }
    const std::array<EnchantRow, 3>& rows = reach.windows.enchanting->rows;
    const auto found = std::ranges::find_if(rows, [netId](const EnchantRow& row) { return row.netId == netId; });
    if (found == rows.end() || found->offers.empty() || found->cost <= 0) {
        return false;
    }
    const auto price = static_cast<std::int32_t>(std::distance(rows.begin(), found)) + 1;
    const Place input{.kind = Place::Kind::Grid, .slot = 0};
    const Place lapis{.kind = Place::Kind::Grid, .slot = 1};
    const std::optional<storage::StoredItemStack> held = stackAt(reach, input);
    const std::optional<storage::StoredItemStack> paying = stackAt(reach, lapis);
    if (!held.has_value()) {
        return false;
    }
    const bool free = !context.canPay || context.canPay(std::numeric_limits<std::int32_t>::max());
    if (!free) {
        if (!context.canPay(std::max(found->cost, price))) {
            return false;
        }
        if (!paying.has_value() || paying->count < price) {
            return false;
        }
        const std::int32_t left = paying->count - price;
        if (!writeStack(reach, lapis, left <= 0 ? std::nullopt : std::optional{withCount(*paying, left)})) {
            return false;
        }
    }
    if (!writeStack(reach, input, world::enchanting::applied(*held, found->offers))) {
        return false;
    }
    reach.enchantCost = price;
    return true;
}

[[nodiscard]] bool cartographyOpen(const BedrockWindows& windows) {
    return windows.open.has_value() && windows.open->type == wire::bedrock::BedrockContainerType::Cartography;
}

[[nodiscard]] bool craftCartography(Reach& reach, const StackRequestContext& context) {
    if (!context.cartographyResult) {
        return false;
    }
    const std::optional<storage::StoredItemStack> map = stackAt(reach, {.kind = Place::Kind::Grid, .slot = 0});
    const std::optional<storage::StoredItemStack> additional = stackAt(reach, {.kind = Place::Kind::Grid, .slot = 1});
    if (!map.has_value() || !additional.has_value()) {
        return false;
    }
    const std::optional<storage::StoredItemStack> made = context.cartographyResult(*map, *additional);
    if (!made.has_value()) {
        return false;
    }
    reach.created = *made;
    reach.cartographyBefore = std::array<std::int32_t, 2>{map->count, additional->count};
    return true;
}

/// parity: `CartographyTableMenu`.
[[nodiscard]] bool cartographyPaid(const Reach& reach) {
    if (!reach.cartographyBefore.has_value()) {
        return true;
    }
    for (std::int32_t slot = 0; slot < 2; ++slot) {
        const std::optional<storage::StoredItemStack> left = stackAt(reach, {.kind = Place::Kind::Grid, .slot = slot});
        const std::int32_t now = left.has_value() ? left->count : 0;
        if (now != (*reach.cartographyBefore)[static_cast<std::size_t>(slot)] - 1) {
            return false;
        }
    }
    return true;
}

} // namespace

StackRequestAnswer applyStackRequest(player::PlayerSession& session, BedrockWindows& windows, world::LoadedWorld* world,
                                     const wire::bedrock::BedrockStackRequest& request,
                                     const StackRequestContext& context) {
    const std::function<void(const storage::StoredItemStack&)>& drop = context.drop;
    Reach reach{.session = session, .windows = windows, .world = world};
    Touched touched;
    for (const BedrockStackAction& action : request.actions) {
        switch (action.type) {
        case BedrockStackActionType::Take:
        case BedrockStackActionType::Place: {
            const Place from = placeOf(action.source, windows);
            // Smithing is the one screen whose result is taken without asking first, so it is worked out on reach.
            if (from.kind == Place::Kind::Created && !reach.created.has_value() && windows.open.has_value() &&
                windows.open->type == wire::bedrock::BedrockContainerType::SmithingTable && !forge(reach, context)) {
                return {};
            }
            if (!moveStack(reach, from, placeOf(action.destination, windows), action.count)) {
                return {};
            }
            touched.add(action.source);
            touched.add(action.destination);
            break;
        }
        case BedrockStackActionType::Swap: {
            const Place from = placeOf(action.source, windows);
            const Place to = placeOf(action.destination, windows);
            if (from.kind == Place::Kind::None || to.kind == Place::Kind::None) {
                return {};
            }
            std::optional<storage::StoredItemStack> held = stackAt(reach, from);
            std::optional<storage::StoredItemStack> other = stackAt(reach, to);
            if (!writeStack(reach, from, std::move(other)) || !writeStack(reach, to, std::move(held))) {
                return {};
            }
            touched.add(action.source);
            touched.add(action.destination);
            break;
        }
        case BedrockStackActionType::Drop: {
            if (!drop) {
                return {};
            }
            const std::optional<storage::StoredItemStack> thrown =
                takeStack(reach, placeOf(action.source, windows), action.count);
            if (!thrown.has_value()) {
                return {};
            }
            reach.dropped.push_back(*thrown);
            touched.add(action.source);
            break;
        }
        case BedrockStackActionType::Destroy:
            if (!takeStack(reach, placeOf(action.source, windows), action.count).has_value()) {
                return {};
            }
            touched.add(action.source);
            break;
        case BedrockStackActionType::Craft: {
            if (cartographyOpen(windows)) {
                if (!craftCartography(reach, context)) {
                    return {};
                }
                break;
            }
            // The row number means only the row: what it puts on was decided when it was rolled.
            if (windows.enchanting.has_value() && windows.open.has_value() &&
                windows.open->type == wire::bedrock::BedrockContainerType::Enchantment) {
                if (!takeEnchantment(reach, action.recipeNetId, context)) {
                    return {};
                }
                touched.add({.container = static_cast<std::int32_t>(BedrockContainerSlot::EnchantingInput)});
                touched.add({.container = static_cast<std::int32_t>(BedrockContainerSlot::EnchantingMaterial)});
                break;
            }
            // The number is the offer's place in the list that was sent, counted from one.
            if (windows.trades.has_value()) {
                if (!takeTrade(reach, action.recipeNetId - 1, std::max(1, action.crafts))) {
                    return {};
                }
                break;
            }
            // The result is this server's answer, not the client's claim: only the number is taken from the line.
            if (!context.resultOf) {
                return {};
            }
            const std::optional<storage::StoredItemStack> made = context.resultOf(action.recipeNetId);
            if (!made.has_value() || action.crafts <= 0) {
                return {};
            }
            // Asking a stack at a time is the same line with a number on it; the result is that many times over.
            reach.created = withCount(*made, made->count * action.crafts);
            break;
        }
        case BedrockStackActionType::Consume: {
            // A trade was paid when picked; acting on these lines would charge for it twice.
            if (reach.trade.has_value()) {
                touched.add(action.source);
                break;
            }
            // What the ingredients come out of is the client's accounting, and stands only if the slot really holds it.
            if (!takeStack(reach, placeOf(action.source, windows), action.count).has_value()) {
                return {};
            }
            touched.add(action.source);
            break;
        }
        case BedrockStackActionType::CraftOptional: {
            // Same line a cartography table sends, whose name box Java does not have: the name goes unused.
            if (cartographyOpen(windows)) {
                if (!craftCartography(reach, context)) {
                    return {};
                }
                break;
            }
            if (!windows.open.has_value() || windows.open->type != wire::bedrock::BedrockContainerType::Anvil ||
                !context.anvilResult) {
                return {};
            }
            const Place input{.kind = Place::Kind::Grid, .slot = 0};
            const Place material{.kind = Place::Kind::Grid, .slot = 1};
            const std::optional<storage::StoredItemStack> left = stackAt(reach, input);
            const std::optional<storage::StoredItemStack> right = stackAt(reach, material);
            std::optional<std::string> typed;
            if (action.textIndex >= 0 && static_cast<std::size_t>(action.textIndex) < request.typed.size()) {
                typed = request.typed[static_cast<std::size_t>(action.textIndex)];
            }
            const world::anvils::Result made =
                context.anvilResult(left.has_value() ? &*left : nullptr, right.has_value() ? &*right : nullptr, typed);
            // parity: `AnvilMenu.mayPickup`: there has to be a price, and it has to be payable.
            if (!made.output.has_value() || made.cost <= 0 || (context.canPay && !context.canPay(made.cost))) {
                return {};
            }
            // parity: `AnvilMenu.onTake`.
            if (!writeStack(reach, input, std::nullopt)) {
                return {};
            }
            if (made.materialCount > 0 && right.has_value() && right->count > made.materialCount) {
                if (!writeStack(reach, material, withCount(*right, right->count - made.materialCount))) {
                    return {};
                }
            } else if (!made.onlyRenaming || made.materialCount > 0) {
                if (!writeStack(reach, material, std::nullopt)) {
                    return {};
                }
            }
            reach.created = *made.output;
            reach.anvilCost = made.cost;
            break;
        }
        case BedrockStackActionType::CraftGrindstone: {
            if (!windows.open.has_value() || windows.open->type != wire::bedrock::BedrockContainerType::Grindstone ||
                !context.grindstoneResult) {
                return {};
            }
            const Place input{.kind = Place::Kind::Grid, .slot = 0};
            const Place addition{.kind = Place::Kind::Grid, .slot = 1};
            const std::optional<storage::StoredItemStack> left = stackAt(reach, input);
            const std::optional<storage::StoredItemStack> right = stackAt(reach, addition);
            const world::grindstones::Result made =
                context.grindstoneResult(left.has_value() ? &*left : nullptr, right.has_value() ? &*right : nullptr);
            if (!made.output.has_value()) {
                return {};
            }
            if (!writeStack(reach, input, std::nullopt) || !writeStack(reach, addition, std::nullopt)) {
                return {};
            }
            reach.created = *made.output;
            reach.grindstoneWorth = made.experience;
            break;
        }
        case BedrockStackActionType::BeaconPayment: {
            // parity: `BeaconMenu.updateEffects`.
            if (!windows.open.has_value() || windows.open->type != wire::bedrock::BedrockContainerType::Beacon) {
                return {};
            }
            const Place payment{.kind = Place::Kind::Grid, .slot = 0};
            const std::optional<storage::StoredItemStack> paying = stackAt(reach, payment);
            if (!paying.has_value()) {
                return {};
            }
            if (!writeStack(reach, payment,
                            paying->count > 1 ? withCount(*paying, paying->count - 1)
                                              : std::optional<storage::StoredItemStack>{})) {
                return {};
            }
            // That edition numbers the effects one higher and calls nothing zero.
            const auto ours = [](const std::int32_t theirs) {
                return theirs == 0 ? std::optional<std::int32_t>{} : std::optional{theirs - 1};
            };
            reach.beacon =
                BeaconChoice{.primary = ours(action.primaryEffect), .secondary = ours(action.secondaryEffect)};
            break;
        }
        case BedrockStackActionType::CraftLoom: {
            if (!windows.open.has_value() || windows.open->type != wire::bedrock::BedrockContainerType::Loom ||
                !context.loomResult) {
                return {};
            }
            const Place banner{.kind = Place::Kind::Grid, .slot = 0};
            const Place dye{.kind = Place::Kind::Grid, .slot = 1};
            const Place pattern{.kind = Place::Kind::Grid, .slot = 2};
            const std::optional<storage::StoredItemStack> woven = stackAt(reach, banner);
            const std::optional<storage::StoredItemStack> colour = stackAt(reach, dye);
            const std::optional<storage::StoredItemStack> named = stackAt(reach, pattern);
            if (!woven.has_value() || !colour.has_value()) {
                return {};
            }
            const std::optional<storage::StoredItemStack> made =
                context.loomResult(*woven, *colour, named.has_value() ? &*named : nullptr, action.pattern);
            if (!made.has_value()) {
                return {};
            }
            // parity: `LoomMenu.ResultSlot.onTake`: one banner and one dye, and the pattern stays.
            const auto eat = [&reach](const Place& where, const storage::StoredItemStack& held) {
                return writeStack(reach, where,
                                  held.count > 1 ? withCount(held, held.count - 1)
                                                 : std::optional<storage::StoredItemStack>{});
            };
            if (!eat(banner, *woven) || !eat(dye, *colour)) {
                return {};
            }
            reach.created = *made;
            break;
        }
        case BedrockStackActionType::CraftResults:
            break;
        default:
            return {};
        }
    }
    // The client asks for both in one request, so a half left over means the whole is refused.
    if ((reach.trade.has_value() || reach.anvilCost.has_value() || reach.grindstoneWorth.has_value()) &&
        reach.created.has_value()) {
        return {};
    }
    if (!cartographyPaid(reach)) {
        return {};
    }
    const std::vector<BedrockResponseContainer> containers = touched.answer(reach);
    commit(reach, drop);
    StackRequestAnswer answer{.accepted = true, .containers = containers};
    if (reach.trade.has_value() && windows.trades.has_value()) {
        // The cells hold the villager's payment, not the player's: it goes with the trade, not back into the bag.
        windows.grid[0].reset();
        windows.grid[1].reset();
        world::trades::Offer& offer = windows.trades->offers[static_cast<std::size_t>(*reach.trade)];
        for (std::int32_t at = 0; at < reach.trades; ++at) {
            world::trades::take(offer);
        }
        answer.tradedWith = windows.trades->entityId;
        answer.tradeExperience = offer.experience * reach.trades;
    }
    answer.anvilCost = reach.anvilCost;
    answer.grindstoneWorth = reach.grindstoneWorth;
    answer.enchantCost = reach.enchantCost;
    answer.beacon = reach.beacon;
    return answer;
}

} // namespace basalt::server::runtime::bedrock
