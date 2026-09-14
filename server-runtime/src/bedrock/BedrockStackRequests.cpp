#include <basalt/server/runtime/bedrock/BedrockStackRequests.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
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

/// Where that edition starts numbering a table's grid. parity: Geyser's own `slot + 31`, which is
/// the number a client sends for the first cell.
constexpr std::int32_t kFirstGridSlot = 32;

/// Where one end of a move really is. Bedrock names a window and a slot of it; this server has one
/// list of slots per player, one stack being dragged, a table's grid and whatever block is open.
struct Place {
    enum class Kind : std::uint8_t { None, Storage, Cursor, Block, Grid };

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
        // That edition numbers a table's nine cells from 32, left to right and then down, which is
        // the same order as here with the number moved.
        const std::int32_t cell = named.slot - kFirstGridSlot;
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
    case BedrockContainerSlot::Cursor:
        return {.kind = Place::Kind::Cursor};
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
};

[[nodiscard]] std::optional<storage::StoredItemStack> stackAt(const Reach& reach, const Place& place) {
    switch (place.kind) {
    case Place::Kind::Storage: {
        const auto staged = reach.inventory.find(place.slot);
        return staged == reach.inventory.end() ? reach.session.inventory.at(place.slot) : staged->second;
    }
    case Place::Kind::Cursor:
        return reach.cursor.has_value() ? *reach.cursor : reach.windows.cursor;
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

} // namespace

StackRequestAnswer applyStackRequest(player::PlayerSession& session, BedrockWindows& windows, world::LoadedWorld* world,
                                     const wire::bedrock::BedrockStackRequest& request,
                                     const std::function<void(const storage::StoredItemStack&)>& drop) {
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
        default:
            // Crafting and everything a block's own machinery does: not moved by this build yet.
            return {};
        }
    }
    const std::vector<BedrockResponseContainer> containers = touched.answer(reach);
    commit(reach, drop);
    return {.accepted = true, .containers = containers};
}

} // namespace basalt::server::runtime::bedrock
