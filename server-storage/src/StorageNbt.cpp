#include <basalt/server/storage/StorageNbt.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::storage {
namespace {
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr std::string_view kVersionKey = "Version";
constexpr std::string_view kSizeKey = "Size";
constexpr std::string_view kSlotsKey = "Slots";
constexpr std::string_view kSlotKey = "Slot";
constexpr std::string_view kIdKey = "id";
constexpr std::string_view kCountKey = "count";
constexpr std::string_view kComponentsKey = "components";
constexpr std::string_view kCountersKey = "Counters";

[[nodiscard]] std::expected<NbtCompound, StorageFormatError> rootOf(std::span<const std::uint8_t> bytes,
                                                                    std::string_view what, std::int32_t formatVersion) {
    const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::read(bytes);
    if (!read.has_value()) {
        return std::unexpected(StorageFormatError{std::string{what} + " blob is not readable NBT"});
    }
    const NbtCompound* root = read->tag.get<NbtCompound>();
    if (root == nullptr) {
        return std::unexpected(StorageFormatError{std::string{what} + " blob is not a compound"});
    }
    const std::int32_t version = root->getInt(kVersionKey);
    if (version != formatVersion) {
        return std::unexpected(StorageFormatError{std::string{what} + " blob is format version " +
                                                  std::to_string(version) + ", this build reads " +
                                                  std::to_string(formatVersion)});
    }
    return *root;
}
} // namespace

std::vector<std::uint8_t> inventoryNbt::encode(const StoredInventory& inventory) {
    NbtCompound root;
    root.putInt(kVersionKey, kFormatVersion);
    root.putInt(kSizeKey, static_cast<std::int32_t>(inventory.slots.size()));
    NbtList occupied{NbtType::Compound};
    for (std::size_t index = 0; index < inventory.slots.size(); ++index) {
        const std::optional<StoredItemStack>& stack = inventory.slots[index];
        if (!stack.has_value()) {
            continue;
        }
        NbtCompound entry;
        entry.putInt(kSlotKey, static_cast<std::int32_t>(index));
        entry.putString(kIdKey, stack->id);
        entry.putInt(kCountKey, stack->count);
        if (!stack->components.empty()) {
            entry.set(kComponentsKey, NbtTag::ofCompound(stack->components));
        }
        occupied.push(NbtTag::ofCompound(std::move(entry)));
    }
    root.set(kSlotsKey, NbtTag::ofList(std::move(occupied)));
    return wire::nbt::nbtIO::write(NbtTag::ofCompound(std::move(root)));
}

std::expected<StoredInventory, StorageFormatError> inventoryNbt::decode(std::span<const std::uint8_t> bytes) {
    const std::expected<NbtCompound, StorageFormatError> root = rootOf(bytes, "inventory", kFormatVersion);
    if (!root.has_value()) {
        return std::unexpected(root.error());
    }
    const std::int32_t size = root->getInt(kSizeKey);
    StoredInventory inventory = StoredInventory::empty(std::max(size, 0));
    const NbtTag* slots = root->find(kSlotsKey);
    const NbtList* list = slots == nullptr ? nullptr : slots->get<NbtList>();
    if (list == nullptr) {
        return inventory;
    }
    for (const NbtTag& element : *list) {
        const NbtCompound* entry = element.get<NbtCompound>();
        if (entry == nullptr) {
            continue;
        }
        const std::int32_t slot = entry->getInt(kSlotKey);
        if (slot < 0 || slot >= size) {
            return std::unexpected(StorageFormatError{"inventory blob has an item in slot " + std::to_string(slot) +
                                                      " of " + std::to_string(size)});
        }
        const std::int32_t count = entry->getInt(kCountKey);
        if (count <= 0) {
            return std::unexpected(StorageFormatError{"inventory blob has " + std::to_string(count) +
                                                      " items in slot " + std::to_string(slot)});
        }
        NbtCompound components;
        if (const NbtTag* found = entry->find(kComponentsKey); found != nullptr) {
            if (const NbtCompound* value = found->get<NbtCompound>(); value != nullptr) {
                components = *value;
            }
        }
        inventory.slots[static_cast<std::size_t>(slot)] = StoredItemStack{
            .id = std::string{entry->getString(kIdKey)}, .count = count, .components = std::move(components)};
    }
    return inventory;
}

std::vector<std::uint8_t> statsNbt::encode(const StoredStats& stats) {
    if (stats.isEmpty()) {
        return {};
    }
    NbtCompound root;
    root.putInt(kVersionKey, kFormatVersion);
    NbtCompound counters;
    for (const auto& [key, value] : stats.counters) {
        if (value != 0) {
            counters.putLong(key, value);
        }
    }
    root.set(kCountersKey, NbtTag::ofCompound(std::move(counters)));
    return wire::nbt::nbtIO::write(NbtTag::ofCompound(std::move(root)));
}

std::expected<StoredStats, StorageFormatError> statsNbt::decode(std::span<const std::uint8_t> bytes) {
    const std::expected<NbtCompound, StorageFormatError> root = rootOf(bytes, "stats", kFormatVersion);
    if (!root.has_value()) {
        return std::unexpected(root.error());
    }
    StoredStats stats;
    const NbtTag* found = root->find(kCountersKey);
    const NbtCompound* counters = found == nullptr ? nullptr : found->get<NbtCompound>();
    if (counters == nullptr) {
        return stats;
    }
    for (const std::string& key : counters->names()) {
        const std::int64_t value = counters->getLong(key);
        if (value != 0) {
            stats.counters.emplace(key, value);
        }
    }
    return stats;
}
} // namespace basalt::server::storage
