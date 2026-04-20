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
struct StoredItemStack {
    std::string id;
    std::int32_t count = 0;
    wire::nbt::NbtCompound components;

    friend bool operator==(const StoredItemStack&, const StoredItemStack&) = default;
};

struct StoredInventory {
    static constexpr std::int32_t kPlayerSlots = 41;

    std::vector<std::optional<StoredItemStack>> slots;

    [[nodiscard]] static StoredInventory empty(std::int32_t size = kPlayerSlots);

    [[nodiscard]] bool isEmpty() const;
};

enum class StoredGamemode : std::uint8_t { Survival, Creative, Adventure, Spectator };

[[nodiscard]] StoredGamemode gamemodeById(std::int32_t id);

struct StoredStats {
    std::map<std::string, std::int64_t, std::less<>> counters;

    [[nodiscard]] std::int64_t get(std::string_view key) const;

    [[nodiscard]] bool isEmpty() const noexcept { return counters.empty(); }

    [[nodiscard]] StoredStats with(std::string_view key, std::int64_t amount) const;
};

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
// parity: `Stats.ITEM_USED`, keyed by the item's id.
[[nodiscard]] std::string used(std::string_view item);
[[nodiscard]] std::string crafted(std::string_view item);
[[nodiscard]] std::string broken(std::string_view item);
[[nodiscard]] std::string pickedUp(std::string_view item);
[[nodiscard]] std::string dropped(std::string_view item);
[[nodiscard]] std::string killed(std::string_view entityType);
[[nodiscard]] std::string killedBy(std::string_view entityType);

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

// parity: `Stats`' distance counters.
[[nodiscard]] const std::string& sprintOneCm();
[[nodiscard]] const std::string& crouchOneCm();
[[nodiscard]] const std::string& swimOneCm();
[[nodiscard]] const std::string& walkOnWaterOneCm();
[[nodiscard]] const std::string& walkUnderWaterOneCm();
[[nodiscard]] const std::string& climbOneCm();
[[nodiscard]] const std::string& flyOneCm();
} // namespace stats

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
    std::optional<std::string> group;
    StoredInventory inventory = StoredInventory::empty();
    StoredInventory enderChest = StoredInventory::empty(kEnderChestSlots);
    StoredStats stats;
    // parity: vanilla keeps tags on the entity, not on the player data.
    std::set<std::string, std::less<>> tags;
    // parity: 26.3 `ServerPlayer.postEffects`, in the order `/posteffect` added them.
    std::vector<std::string> postEffects;
    // parity: `Player.wardenSpawnTracker`.
    std::array<std::int32_t, 3> wardenSpawnTracker{};
    wire::nbt::NbtCompound entityData;
};
} // namespace basalt::server::storage
