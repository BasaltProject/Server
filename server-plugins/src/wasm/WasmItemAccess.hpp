#pragma once

#include "WasmItemValues.hpp"

#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginWindow.hpp>
#include <basalt/server/plugins/PluginWorldRef.hpp>
#include <basalt/server/registry/Items.hpp>

#include <array>
#include <memory>

namespace basalt::server::plugins::wasm::items {

inline constexpr std::int32_t kEnderSlots = 27;
inline constexpr std::int32_t kWindowPlayerSlots = 36;

enum class Kind : std::uint8_t { Player, Block, Window, Ender, Entity };

/// What an inventory handle points at; resolved afresh on every call, so a gone target answers STALE.
struct Target {
    Kind kind = Kind::Player;
    std::shared_ptr<PluginPlayerRef> player;
    std::shared_ptr<PluginWorldRef> world;
    std::array<std::int32_t, 3> at{};
    std::int32_t container = 0;
};

using InventoryTable = HandleTable<Target>;

struct ContainerSize {
    std::string_view entity;
    std::int32_t slots;
};

inline constexpr std::array<ContainerSize, 17> kContainers{{{"minecraft:chest", 27},
                                                            {"minecraft:trapped_chest", 27},
                                                            {"minecraft:barrel", 27},
                                                            {"minecraft:shulker_box", 27},
                                                            {"minecraft:hopper", 5},
                                                            {"minecraft:dispenser", 9},
                                                            {"minecraft:dropper", 9},
                                                            {"minecraft:furnace", 3},
                                                            {"minecraft:blast_furnace", 3},
                                                            {"minecraft:smoker", 3},
                                                            {"minecraft:brewing_stand", 5},
                                                            {"minecraft:crafter", 9},
                                                            {"minecraft:chiseled_bookshelf", 6},
                                                            {"minecraft:decorated_pot", 1},
                                                            {"minecraft:lectern", 1},
                                                            {"minecraft:jukebox", 1},
                                                            {"minecraft:campfire", 4}}};

[[nodiscard]] inline std::optional<std::int32_t> containerSlots(std::string_view entity) {
    for (const ContainerSize& known : kContainers) {
        if (known.entity == entity) {
            return known.slots;
        }
    }
    return std::nullopt;
}

[[nodiscard]] inline bool sameKind(const PluginItem& left, const PluginItem& right) {
    return left.id == right.id && mergedComponents(left) == mergedComponents(right);
}

[[nodiscard]] inline std::int32_t stackLimit(const PluginItem& item) {
    return std::max(1, registry::itemStacking::maxStackSize(item.id, mergedComponents(item)));
}

/// Slot access to one inventory; the generic operations here work for every kind that is a plain list of slots.
class Access {
public:
    Access() = default;
    Access(const Access&) = delete;
    Access& operator=(const Access&) = delete;
    virtual ~Access() = default;

    [[nodiscard]] virtual std::int32_t size() = 0;
    [[nodiscard]] virtual Slot get(std::int32_t slot) = 0;
    virtual void set(std::int32_t slot, const Slot& item) = 0;

    virtual void commit() {}

    /// Whether slot -1 reads and writes the item on the cursor.
    [[nodiscard]] virtual bool hasCursor() const { return false; }

    /// The slots add and clear touch; a window leaves the player's own part alone.
    [[nodiscard]] virtual std::int32_t ownSlots() { return size(); }

    [[nodiscard]] virtual std::int32_t add(PluginItem item) {
        const std::int32_t own = ownSlots();
        const std::int32_t limit = stackLimit(item);
        for (std::int32_t slot = 0; slot < own && item.count > 0; ++slot) {
            Slot held = get(slot);
            if (held.has_value() && sameKind(*held, item) && held->count < limit) {
                const std::int32_t moved = std::min(item.count, limit - held->count);
                held->count += moved;
                item.count -= moved;
                set(slot, held);
            }
        }
        for (std::int32_t slot = 0; slot < own && item.count > 0; ++slot) {
            if (!get(slot).has_value()) {
                PluginItem placed = item;
                placed.count = std::min(item.count, limit);
                item.count -= placed.count;
                set(slot, placed);
            }
        }
        return item.count;
    }

    [[nodiscard]] virtual std::int32_t remove(std::string_view id, std::int32_t count) {
        std::int32_t left = count;
        for (std::int32_t slot = 0; slot < size() && left > 0; ++slot) {
            Slot held = get(slot);
            if (held.has_value() && held->id == id) {
                const std::int32_t taken = std::min(left, held->count);
                held->count -= taken;
                left -= taken;
                set(slot, held->count > 0 ? held : Slot{});
            }
        }
        return count - left;
    }

    [[nodiscard]] virtual std::int32_t count(std::string_view id) {
        std::int32_t total = 0;
        for (std::int32_t slot = 0; slot < size(); ++slot) {
            const Slot held = get(slot);
            total += held.has_value() && held->id == id ? held->count : 0;
        }
        return total;
    }

    virtual void clear() {
        for (std::int32_t slot = 0; slot < ownSlots(); ++slot) {
            set(slot, std::nullopt);
        }
    }

    [[nodiscard]] virtual std::optional<std::int32_t> heldSlot() { return std::nullopt; }

    virtual bool setHeldSlot(std::int32_t slot) {
        static_cast<void>(slot);
        return false;
    }
};

class PlayerAccess final : public Access {
public:
    explicit PlayerAccess(PluginPlayerRef& player) : inventory_(&player.inventory()) {}

    std::int32_t size() override { return inventory_->size(); }

    Slot get(std::int32_t slot) override { return inventory_->at(slot); }

    void set(std::int32_t slot, const Slot& item) override { inventory_->set(slot, item); }

    std::int32_t add(PluginItem item) override { return inventory_->add(item); }

    std::int32_t remove(std::string_view id, std::int32_t count) override { return inventory_->remove(id, count); }

    std::int32_t count(std::string_view id) override { return inventory_->count(id); }

    void clear() override { inventory_->clear(); }

    std::optional<std::int32_t> heldSlot() override { return inventory_->heldSlot(); }

    bool setHeldSlot(std::int32_t slot) override {
        inventory_->setHeldSlot(slot);
        return true;
    }

private:
    PluginInventory* inventory_;
};

class EnderAccess final : public Access {
public:
    explicit EnderAccess(PluginPlayerRef& player) : player_(&player) {}

    std::int32_t size() override { return kEnderSlots; }

    Slot get(std::int32_t slot) override {
        const std::optional<PluginEntityValue> held = player_->invoke("ender_slot", {std::int64_t{slot}});
        const auto* text = held.has_value() ? std::get_if<std::string>(&*held) : nullptr;
        return text == nullptr ? Slot{} : itemFromSnbt(*text);
    }

    void set(std::int32_t slot, const Slot& item) override {
        static_cast<void>(
            player_->invoke("set_ender_slot", {std::int64_t{slot}, item.has_value() ? snbtOf(*item) : std::string{}}));
    }

private:
    PluginPlayerRef* player_;
};

class WindowAccess final : public Access {
public:
    explicit WindowAccess(PluginWindow& window) : window_(&window) {}

    std::int32_t size() override { return window_->windowInfo().slots; }

    Slot get(std::int32_t slot) override { return slot == -1 ? window_->cursorItem() : window_->windowItem(slot); }

    void set(std::int32_t slot, const Slot& item) override {
        if (slot == -1) {
            window_->setCursorItem(item);
        } else {
            window_->setWindowItem(slot, item);
        }
    }

    bool hasCursor() const override { return true; }

    std::int32_t ownSlots() override { return std::max(0, size() - kWindowPlayerSlots); }

private:
    PluginWindow* window_;
};

/// A container block entity read once, edited in memory and written back by `commit`.
class BlockAccess final : public Access {
public:
    BlockAccess(PluginWorldRef& world, const std::array<std::int32_t, 3>& at, NbtCompound root, std::int32_t slots)
        : world_(&world), at_(at), root_(std::move(root)), slots_(slots) {
        if (const wire::nbt::NbtList* list = root_.getList("Items"); list != nullptr) {
            for (const NbtTag& entry : *list) {
                if (const auto* compound = entry.get<NbtCompound>(); compound != nullptr) {
                    items_.emplace_back(compound->getByte("Slot"), itemOfCompound(*compound));
                }
            }
        }
    }

    std::int32_t size() override { return slots_; }

    Slot get(std::int32_t slot) override {
        for (const auto& [index, item] : items_) {
            if (index == slot) {
                return item;
            }
        }
        return std::nullopt;
    }

    void set(std::int32_t slot, const Slot& item) override {
        std::erase_if(items_, [slot](const auto& entry) { return entry.first == slot; });
        if (item.has_value()) {
            items_.emplace_back(slot, *item);
        }
        dirty_ = true;
    }

    void commit() override {
        if (!dirty_) {
            return;
        }
        std::sort(items_.begin(), items_.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        wire::nbt::NbtList list{wire::nbt::NbtType::Compound};
        for (const auto& [index, item] : items_) {
            NbtCompound entry = compoundOf(item);
            entry.putByte("Slot", static_cast<std::int8_t>(index));
            static_cast<void>(list.push(NbtTag::ofCompound(std::move(entry))));
        }
        root_.set("Items", NbtTag::ofList(std::move(list)));
        static_cast<void>(
            world_->setBlockEntityNbt(at_[0], at_[1], at_[2], wire::nbt::nbtIO::write(NbtTag::ofCompound(root_))));
        dirty_ = false;
    }

private:
    PluginWorldRef* world_;
    std::array<std::int32_t, 3> at_;
    NbtCompound root_;
    std::int32_t slots_;
    std::vector<std::pair<std::int32_t, PluginItem>> items_;
    bool dirty_ = false;
};

/// The block entity at a position when it holds a known container; its id is returned through `slots`.
[[nodiscard]] inline std::unique_ptr<BlockAccess> openBlock(PluginWorldRef& world,
                                                            const std::array<std::int32_t, 3>& at) {
    const std::optional<std::vector<std::uint8_t>> bytes = world.blockEntityNbt(at[0], at[1], at[2]);
    if (!bytes.has_value()) {
        return nullptr;
    }
    const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::read(*bytes);
    const NbtCompound* root = read.has_value() ? read->tag.get<NbtCompound>() : nullptr;
    const std::optional<std::int32_t> slots = root == nullptr ? std::nullopt : containerSlots(root->getString("id"));
    return slots.has_value() ? std::make_unique<BlockAccess>(world, at, *root, *slots) : nullptr;
}
} // namespace basalt::server::plugins::wasm::items
