#include <basalt/server/datafix/DataFixer.hpp>
#include <basalt/server/storage/VanillaPlayerData.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <cstdint>
#include <array>
#include <optional>
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

// parity: `WardenSpawnTracker.CODEC`, three counts that default to zero.
[[nodiscard]] std::array<std::int32_t, 3> wardenTrackerOf(const NbtCompound* tracker) {
    if (tracker == nullptr) {
        return {};
    }
    return {tracker->getInt("ticks_since_last_warning"), tracker->getInt("warning_level"),
            tracker->getInt("cooldown_ticks")};
}

// parity: 26.3 `ServerPlayer.readAdditionalSaveData`'s `post_effects`.
[[nodiscard]] std::vector<std::string> postEffectsOf(const NbtList* list) {
    std::vector<std::string> effects;
    if (list == nullptr) {
        return effects;
    }
    for (const wire::nbt::NbtTag& element : *list) {
        if (const std::string* effect = element.get<std::string>(); effect != nullptr) {
            effects.push_back(*effect);
        }
    }
    return effects;
}

[[nodiscard]] std::optional<StoredItemStack> stackOf(const NbtCompound& item) {
    const std::int32_t count = item.getInt("count");
    if (count <= 0) {
        return std::nullopt;
    }
    NbtCompound components;
    if (const NbtTag* found = item.find("components"); found != nullptr) {
        if (const NbtCompound* value = found->get<NbtCompound>(); value != nullptr) {
            components = *value;
        }
    }
    return StoredItemStack{
        .id = std::string{item.getString("id")}, .count = count, .components = std::move(components)};
}

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
        if (std::optional<StoredItemStack> stack = stackOf(*item); stack.has_value()) {
            inventory.slots[static_cast<std::size_t>(slot)] = std::move(stack);
        }
    }
    return inventory;
}

/// parity: `LivingEntity` keeps worn items and the offhand in `equipment`, one stack per slot, since 1.21.5.
void readEquipment(const NbtCompound* equipment, StoredInventory& inventory) {
    if (equipment == nullptr) {
        return;
    }
    static constexpr std::array<std::pair<std::string_view, std::int32_t>, 5> kSlots{{{"feet", kMainSlots},
                                                                                      {"legs", kMainSlots + 1},
                                                                                      {"chest", kMainSlots + 2},
                                                                                      {"head", kMainSlots + 3},
                                                                                      {"offhand", kMainSlots + 4}}};
    for (const auto& [name, slot] : kSlots) {
        const NbtCompound* held = equipment->getCompound(name);
        if (held == nullptr || static_cast<std::size_t>(slot) >= inventory.slots.size()) {
            continue;
        }
        if (std::optional<StoredItemStack> stack = stackOf(*held); stack.has_value()) {
            inventory.slots[static_cast<std::size_t>(slot)] = std::move(stack);
        }
    }
}

// parity: the `Score` and shoulder parrots a `.dat` keeps.
[[nodiscard]] wire::nbt::NbtCompound entityDataOf(const wire::nbt::NbtCompound& root) {
    wire::nbt::NbtCompound kept;
    for (const std::string_view key : {"Score", "ShoulderEntityLeft", "ShoulderEntityRight"}) {
        if (const wire::nbt::NbtTag* tag = root.find(key); tag != nullptr) {
            kept.set(std::string{key}, *tag);
        }
    }
    return kept;
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
    const NbtCompound* saved = read->tag.get<NbtCompound>();
    if (saved == nullptr) {
        return std::unexpected(StorageFormatError{std::string{uuid} + ".dat is not an NBT compound"});
    }
    NbtCompound upgraded = *saved;
    if (const std::int32_t version = upgraded.getInt("DataVersion"); version > server::datafix::kCurrentVersion) {
        return std::unexpected(StorageFormatError{std::string{uuid} + ".dat was saved by a newer version of the game"});
    } else if (version > 0 && version < server::datafix::kCurrentVersion) {
        server::datafix::upgrade(server::datafix::Domain::Player, upgraded, version);
    }
    const NbtCompound* root = &upgraded;

    const NbtList* position = listOf(*root, "Pos");
    const NbtList* rotation = listOf(*root, "Rotation");
    StoredInventory inventory = readInventory(listOf(*root, "Inventory"), StoredInventory::kPlayerSlots);
    readEquipment(root->getCompound("equipment"), inventory);
    return StoredPlayer{
        .uuid = std::string{uuid},
        .name = std::string{name},
        .lastSeenEpochSeconds = nowEpochSeconds,
        .firstSeenEpochSeconds = nowEpochSeconds,
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
        .inventory = std::move(inventory),
        .enderChest = readInventory(listOf(*root, "EnderItems"), StoredPlayer::kEnderChestSlots),
        .stats = {},
        .tags = {},
        .postEffects = postEffectsOf(listOf(*root, "post_effects")),
        .wardenSpawnTracker = wardenTrackerOf(root->getCompound("warden_spawn_tracker")),
        .entityData = entityDataOf(*root),
    };
}
} // namespace basalt::server::storage::vanillaPlayerData
