#include <basalt/server/runtime/bedrock/BedrockStackRequests.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
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

/// Where that edition starts numbering a grid. A table's nine cells run from 32 and the player's
/// own four from 28: the same window number with two runs of slots inside it.
constexpr std::int32_t kFirstTableSlot = 32;
constexpr std::int32_t kFirstCarriedSlot = 28;
constexpr std::int32_t kCarriedCells = 4;

/// Where one end of a move really is. Bedrock names a window and a slot of it; this server has one
/// list of slots per player, one stack being dragged, a table's grid and whatever block is open.
struct Place {
    enum class Kind : std::uint8_t { None, Storage, Cursor, Block, Grid, Created };

    Kind kind = Kind::None;
    std::int32_t slot = 0;
};

/// Bedrock's own slot numbering for the 36 a player carries puts the hotbar first; this server's
/// puts it last.
[[nodiscard]] std::int32_t carriedSlotOf(const std::int32_t slot) {
    return slot < PlayerInventory::kHotbarSize ? PlayerInventory::kHotbarStart + slot
                                               : slot - PlayerInventory::kHotbarSize;
}

/// Where one end of a move is, given what this player has open: which of the block's own slots a
/// container names is the window's business, not this table's.
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
        // Both grids are counted left to right and then down, which is this server's own order.
        // The four a player carries land on the first four cells: only one grid is ever open.
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
    case BedrockContainerSlot::AnvilInput:
        // An anvil's two slots belong to the screen, so they live where a grid's first cells do:
        // only one screen is ever open.
        return {.kind = Place::Kind::Grid, .slot = 0};
    case BedrockContainerSlot::AnvilMaterial:
        return {.kind = Place::Kind::Grid, .slot = 1};
    case BedrockContainerSlot::StonecutterInput:
        // One slot, which lives where the grid's first cell does: only one screen is ever open.
        return {.kind = Place::Kind::Grid, .slot = 0};
    case BedrockContainerSlot::TradeIngredientA:
        // What a trade is being paid with. It is laid in the first two cells for the same reason:
        // nothing else is open behind a merchant's book.
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
        // What a craft just made, which is nowhere until the line that takes it says where.
        return {.kind = Place::Kind::Created};
    default:
        return {};
    }
}

/// Everything one request reaches, and everything it has changed so far.
///
/// Nothing is written where it belongs until every line of the request has worked: a request is all
/// or nothing on the client too, so a half-applied one leaves the two sides disagreeing about a
/// chest. The staged writes here are what makes "refused" mean the world did not move.
struct Reach {
    player::PlayerSession& session;
    BedrockWindows& windows;
    world::LoadedWorld* world = nullptr;
    std::map<std::int32_t, std::optional<storage::StoredItemStack>> inventory;
    std::map<std::int32_t, std::optional<storage::StoredItemStack>> block;
    std::map<std::int32_t, std::optional<storage::StoredItemStack>> grid;
    std::optional<std::optional<storage::StoredItemStack>> cursor;
    std::vector<storage::StoredItemStack> dropped;
    /// What the recipe named by the last craft line makes. It exists only between that line and the
    /// one that takes it, which is how this edition hands a result over.
    std::optional<storage::StoredItemStack> created;
    /// Which of the open merchant's offers this request is buying, and how many times over. The
    /// offer is not counted as used until every line of the request has worked.
    std::optional<std::int32_t> trade;
    std::int32_t trades = 0;
    /// What the open anvil charged, once the request has taken what it made.
    std::optional<std::int32_t> anvilCost;
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

/// False when the write has nowhere to land, which ends the request.
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

/// Puts everything the request worked out where it belongs. Runs only once the whole request has.
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
    }
}

[[nodiscard]] storage::StoredItemStack withCount(storage::StoredItemStack stack, const std::int32_t count) {
    stack.count = count;
    return stack;
}

/// Whether two stacks are the same thing, so one can be poured into the other.
[[nodiscard]] bool sameKind(const storage::StoredItemStack& a, const storage::StoredItemStack& b) {
    return a.id == b.id && a.components == b.components;
}

/// Which windows a request touched, so the answer names them and nothing else.
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

/// Pours `count` from one place into another, or says it cannot.
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
        if (!sameKind(*target, *source) || target->count + count > PlayerInventory::kMaxStack) {
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

/// Takes `count` out of a place and hands it over, or says it cannot.
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

/// Takes what an offer asks for out of whatever slots hold it and lays it in the cell that edition
/// draws it in. False when it is not all there, which leaves the staged writes to be thrown away
/// with the rest of the request.
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
///
/// parity: `MerchantMenu.clickMenuButton` over `MerchantResultSlot.onTake`: the cost comes out of
/// the inventory when the offer is picked, and what it buys is what the next line takes.
[[nodiscard]] bool takeTrade(Reach& reach, const std::int32_t index, const std::int32_t times) {
    if (!reach.windows.trades.has_value() || index < 0 || times <= 0 ||
        static_cast<std::size_t>(index) >= reach.windows.trades->offers.size()) {
        return false;
    }
    const world::trades::Offer& offer = reach.windows.trades->offers[static_cast<std::size_t>(index)];
    // parity: `MerchantOffer.isOutOfStock`: an offer that has run out is not sold again until the
    // merchant restocks, and asking for more than is left buys none of them.
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
        case BedrockStackActionType::Place:
            if (!moveStack(reach, placeOf(action.source, windows), placeOf(action.destination, windows),
                           action.count)) {
                return {};
            }
            touched.add(action.source);
            touched.add(action.destination);
            break;
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
            // A merchant's book names a trade where a table names a recipe, and the number is the
            // offer's place in the list that was sent, counted from one.
            if (windows.trades.has_value()) {
                if (!takeTrade(reach, action.recipeNetId - 1, std::max(1, action.crafts))) {
                    return {};
                }
                break;
            }
            // What the recipe makes is this server's answer, not the client's claim: the number is
            // the only thing taken from the line.
            if (!context.resultOf) {
                return {};
            }
            const std::optional<storage::StoredItemStack> made = context.resultOf(action.recipeNetId);
            if (!made.has_value() || action.crafts <= 0) {
                return {};
            }
            // Asking for it a stack at a time is the same line with a number on it: what it makes
            // is that many times over, and the lines that follow take it in whatever helpings fit.
            reach.created = withCount(*made, made->count * action.crafts);
            break;
        }
        case BedrockStackActionType::Consume: {
            // A trade was paid for when it was picked, so these lines are the client saying again
            // what it thinks that cost. Acting on them would charge for the same trade twice.
            if (reach.trade.has_value()) {
                touched.add(action.source);
                break;
            }
            // The ingredients, one line each. What they come out of is the client's own accounting,
            // and it only stands if the slot really holds what it says.
            if (!takeStack(reach, placeOf(action.source, windows), action.count).has_value()) {
                return {};
            }
            touched.add(action.source);
            break;
        }
        case BedrockStackActionType::CraftOptional: {
            // An anvil, which names no recipe: what it makes is worked out from what is in it, and
            // the name is whatever was typed into the box.
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
            // parity: `AnvilMenu.onTake`: the first slot goes whatever happens, the second only as
            // far as the repair ate into it.
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
        case BedrockStackActionType::CraftResults:
            // The client repeating what it thinks it made, which this server already knows.
            break;
        default:
            // Everything a block's own machinery does: not moved by this build yet.
            return {};
        }
    }
    // Work nobody took the result of is work nobody paid for: the client asks for both in one
    // request, so a half of it left over means the whole of it is refused.
    if ((reach.trade.has_value() || reach.anvilCost.has_value()) && reach.created.has_value()) {
        return {};
    }
    const std::vector<BedrockResponseContainer> containers = touched.answer(reach);
    commit(reach, drop);
    StackRequestAnswer answer{.accepted = true, .containers = containers};
    if (reach.trade.has_value() && windows.trades.has_value()) {
        // The two cells hold the villager's payment, not the player's: whatever the client left in
        // them goes with the trade rather than back into the bag when the book closes.
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
    return answer;
}

} // namespace basalt::server::runtime::bedrock
