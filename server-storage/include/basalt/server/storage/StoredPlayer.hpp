#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::storage {

/// One item in a slot: what it is, how many, and whatever else the game hung off it.
///
/// `components` stays an NBT compound rather than field by field: what an item can carry changes
/// every version, and a persistence layer that enumerates them silently drops what it has not been
/// taught. Round-tripping the compound whole cannot lose anything.
struct StoredItemStack {
    std::string id;
    std::int32_t count = 0;
    wire::nbt::NbtCompound components;

    friend bool operator==(const StoredItemStack&, const StoredItemStack&) = default;
};

/// A player's slots. Empty slots are absent, and the list keeps its length so slot numbers survive:
/// an inventory is addressed by index, so compacting it would silently move everything.
struct StoredInventory {
    /// A player's main inventory: 36 slots, hotbar included, plus 4 armour and 1 offhand.
    static constexpr std::int32_t kPlayerSlots = 41;

    std::vector<std::optional<StoredItemStack>> slots;

    [[nodiscard]] static StoredInventory empty(std::int32_t size = kPlayerSlots);

    [[nodiscard]] bool isEmpty() const;
};

/// The four game modes, by the id the protocol and the database both use.
enum class StoredGamemode : std::uint8_t { Survival, Creative, Adventure, Spectator };

/// An id outside the four is survival, not a failure: the column is an integer and a corrupt row
/// should not stop somebody logging in.
[[nodiscard]] StoredGamemode gamemodeById(std::int32_t id);

/// A player's counters, keyed as vanilla keys them (`minecraft:custom/minecraft:deaths`) so a
/// `stats/<uuid>.json` import needs no translation. 64-bit where vanilla's are 32: `walk_one_cm`
/// overflows an int in a few hundred hours. Nothing on the wire carries these yet.
struct StoredStats {
    std::map<std::string, std::int64_t, std::less<>> counters;

    [[nodiscard]] std::int64_t get(std::string_view key) const;

    [[nodiscard]] bool isEmpty() const noexcept { return counters.empty(); }

    /// Adds to a counter, which is the only way one ever moves.
    [[nodiscard]] StoredStats with(std::string_view key, std::int64_t amount) const;
};

/// The category every counter that is not about a block or an item goes in.
namespace stats {

inline constexpr std::string_view kCustom = "minecraft:custom";
inline constexpr std::string_view kMined = "minecraft:mined";
inline constexpr std::string_view kUsed = "minecraft:used";
inline constexpr std::string_view kCrafted = "minecraft:crafted";
inline constexpr std::string_view kBroken = "minecraft:broken";
inline constexpr std::string_view kPickedUp = "minecraft:picked_up";
inline constexpr std::string_view kDropped = "minecraft:dropped";
inline constexpr std::string_view kKilled = "minecraft:killed";
inline constexpr std::string_view kKilledBy = "minecraft:killed_by";

[[nodiscard]] std::string custom(std::string_view name);
[[nodiscard]] std::string mined(std::string_view block);
/// parity: `Stats.ITEM_USED`, keyed by the item's id.
[[nodiscard]] std::string used(std::string_view item);
[[nodiscard]] std::string crafted(std::string_view item);
[[nodiscard]] std::string broken(std::string_view item);
[[nodiscard]] std::string pickedUp(std::string_view item);
[[nodiscard]] std::string dropped(std::string_view item);
[[nodiscard]] std::string killed(std::string_view entityType);
[[nodiscard]] std::string killedBy(std::string_view entityType);

// The handful this server can honestly count today. Vanilla has around a hundred more, and each one
// is a place in the code that has to notice something happening: they arrive with the systems that
// make them observable, not before.
[[nodiscard]] const std::string& deaths();
[[nodiscard]] const std::string& playTime();
[[nodiscard]] const std::string& timeSinceRest();
[[nodiscard]] const std::string& walkOneCm();
[[nodiscard]] const std::string& damageTaken();
[[nodiscard]] const std::string& damageDealt();
[[nodiscard]] const std::string& mobKills();
[[nodiscard]] const std::string& jump();
[[nodiscard]] const std::string& fallOneCm();
[[nodiscard]] const std::string& enchantItem();
[[nodiscard]] const std::string& animalsBred();
[[nodiscard]] const std::string& fishCaught();

// parity: `Stats`' distance counters. One step counts towards exactly one of them.
[[nodiscard]] const std::string& sprintOneCm();
[[nodiscard]] const std::string& crouchOneCm();
[[nodiscard]] const std::string& swimOneCm();
[[nodiscard]] const std::string& walkOnWaterOneCm();
[[nodiscard]] const std::string& walkUnderWaterOneCm();
[[nodiscard]] const std::string& climbOneCm();
[[nodiscard]] const std::string& flyOneCm();

} // namespace stats

/// Everything the server keeps about a player between sessions.
///
/// The split is deliberate and mirrors the table: scalars are indexable columns, the two inventories
/// and the stats become blobs. Nothing in here refers to a world directory, which is what lets a
/// world be deleted without touching a single row.
struct StoredPlayer {
    static constexpr std::int32_t kEnderChestSlots = 27;

    std::string uuid;
    std::string name;
    std::int64_t lastSeenEpochSeconds = 0;
    std::int64_t firstSeenEpochSeconds = 0;
    std::int64_t playTimeTicks = 0;
    std::string world;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;
    float health = 0.0F;
    std::int32_t food = 0;
    float saturation = 0.0F;
    float exhaustion = 0.0F;
    std::int32_t xpLevel = 0;
    float xpProgress = 0.0F;
    std::int32_t xpTotal = 0;
    StoredGamemode gamemode = StoredGamemode::Survival;
    /// Which permission group they are in, or absent for whichever group is currently the default.
    std::optional<std::string> group;
    StoredInventory inventory = StoredInventory::empty();
    StoredInventory enderChest = StoredInventory::empty(kEnderChestSlots);
    StoredStats stats;
    /// Scoreboard tags, as `/tag` set them. parity: vanilla keeps these with the entity.
    std::set<std::string, std::less<>> tags;
    /// parity: 26.3 `ServerPlayer.postEffects`, in the order `/posteffect` added them.
    std::vector<std::string> postEffects;
    /// parity: `Player.wardenSpawnTracker`: ticks since the last warning, the level and the cooldown.
    std::array<std::int32_t, 3> wardenSpawnTracker{};
    /// What the player's entity keeps that has no column of its own, under vanilla's own keys:
    /// `Score`, `ShoulderEntityLeft` and `ShoulderEntityRight`.
    wire::nbt::NbtCompound entityData;
};

} // namespace basalt::server::storage
