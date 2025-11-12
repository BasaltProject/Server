#include <basalt/server/storage/VanillaPlayerData.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <string>
#include <utility>

namespace basalt::server::storage::vanillaPlayerData {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

constexpr std::int32_t kMainSlots = 36;
constexpr std::int32_t kArmourBase = 100;
constexpr std::int32_t kOffhand = -106;

[[nodiscard]] double asDouble(const NbtList* list, std::size_t index) {
    if (list == nullptr || index >= list->size()) {
        return 0.0;
    }
    const NbtTag& tag = (*list)[index];
    if (const double* value = tag.get<double>(); value != nullptr) {
        return *value;
    }
    if (const float* value = tag.get<float>(); value != nullptr) {
        return static_cast<double>(*value);
    }
    return 0.0;
}

[[nodiscard]] const NbtList* listOf(const NbtCompound& root, std::string_view key) {
    const NbtTag* found = root.find(key);
    return found == nullptr ? nullptr : found->get<NbtList>();
}

/// Vanilla's inventory is a list of item compounds each carrying its own `Slot` byte, with armour at
/// slots 100 to 103 and the offhand at -106. Those are folded into the flat layout used here,
/// because a slot number that is negative or above the inventory size is vanilla's own encoding
/// trick, not a real position.
[[nodiscard]] StoredInventory readInventory(const NbtList* items, std::int32_t size) {
    StoredInventory inventory = StoredInventory::empty(size);
    if (items == nullptr) {
        return inventory;
    }
    for (const NbtTag& element : *items) {
        const NbtCompound* item = element.get<NbtCompound>();
        if (item == nullptr) {
            continue;
        }
        const std::int32_t vanillaSlot = item->getByte("Slot");
        std::int32_t slot = 0;
        if (vanillaSlot >= 0 && vanillaSlot < kMainSlots) {
            slot = vanillaSlot;
        } else if (vanillaSlot >= kArmourBase && vanillaSlot < kArmourBase + 4) {
            slot = kMainSlots + (vanillaSlot - kArmourBase);
        } else if (vanillaSlot == kOffhand) {
            slot = kMainSlots + 4;
        } else {
            continue;
        }
        if (slot < 0 || slot >= size) {
            continue;
        }
        const std::int32_t count = item->getInt("count");
        if (count <= 0) {
            continue;
        }
        NbtCompound components;
        if (const NbtTag* found = item->find("components"); found != nullptr) {
            if (const NbtCompound* value = found->get<NbtCompound>(); value != nullptr) {
                components = *value;
            }
        }
        inventory.slots[static_cast<std::size_t>(slot)] = StoredItemStack{
            .id = std::string{item->getString("id")}, .count = count, .components = std::move(components)};
    }
    return inventory;
}

} // namespace

std::expected<StoredPlayer, StorageFormatError> read(std::string_view uuid, std::string_view name,
                                                     std::span<const std::uint8_t> gzippedBytes,
                                                     const Decompressor& decompress, std::int64_t nowEpochSeconds,
                                                     std::string_view worldName) {
    const std::vector<std::uint8_t> plain = decompress(gzippedBytes);
    const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::read(plain);
    if (!read.has_value()) {
        return std::unexpected(StorageFormatError{std::string{uuid} + ".dat is not readable NBT"});
    }
    const NbtCompound* root = read->tag.get<NbtCompound>();
    if (root == nullptr) {
        return std::unexpected(StorageFormatError{std::string{uuid} + ".dat is not an NBT compound"});
    }

    const NbtList* position = listOf(*root, "Pos");
    const NbtList* rotation = listOf(*root, "Rotation");
    return StoredPlayer{
        .uuid = std::string{uuid},
        .name = std::string{name},
        .lastSeenEpochSeconds = nowEpochSeconds,
        .firstSeenEpochSeconds = nowEpochSeconds,
        // Vanilla keeps play time in the statistics file, not here, so an import starts the counter
        // rather than inventing one.
        .playTimeTicks = 0,
        .world = std::string{worldName},
        .x = asDouble(position, 0),
        .y = asDouble(position, 1),
        .z = asDouble(position, 2),
        .yaw = static_cast<float>(asDouble(rotation, 0)),
        .pitch = static_cast<float>(asDouble(rotation, 1)),
        .health = root->getFloat("Health"),
        .food = root->getInt("foodLevel"),
        .saturation = root->getFloat("foodSaturationLevel"),
        .exhaustion = root->getFloat("foodExhaustionLevel"),
        .xpLevel = root->getInt("XpLevel"),
        .xpProgress = root->getFloat("XpP"),
        .xpTotal = root->getInt("XpTotal"),
        .gamemode = gamemodeById(root->getInt("playerGameType")),
        .group = std::nullopt,
        .inventory = readInventory(listOf(*root, "Inventory"), StoredInventory::kPlayerSlots),
        .enderChest = readInventory(listOf(*root, "EnderItems"), StoredPlayer::kEnderChestSlots),
        .stats = {},
        .tags = {},
    };
}

} // namespace basalt::server::storage::vanillaPlayerData
