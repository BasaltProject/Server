#pragma once

#include <basalt/server/runtime/config/ConfigDirectory.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::config {

/// One entity type's tweaks; an absent field is vanilla's own value.
struct MobOverride {
    std::optional<double> maxHealth;
    std::optional<double> attackDamage;
    std::optional<double> movementSpeed;
    std::optional<double> followRange;
    std::optional<double> armor;
    std::optional<double> armorToughness;
    std::optional<double> knockbackResistance;
    std::optional<double> attackKnockback;
    std::optional<double> flyingSpeed;
    std::optional<double> jumpStrength;
    std::optional<double> scale;
    std::optional<double> safeFallDistance;
    std::optional<double> fallDamageMultiplier;
    std::optional<double> explosionKnockbackResistance;
    std::optional<double> oxygenBonus;
    std::optional<double> experienceMultiplier;

    std::optional<bool> burnInDaylight;
    std::optional<bool> noAi;
    std::optional<bool> persistent;
    std::optional<bool> canPickUpLoot;
    std::optional<bool> noGravity;
    std::optional<bool> collidable;
    std::optional<bool> canSwim;
    std::optional<bool> opensDoors;
    std::optional<bool> passesDoors;
    std::optional<bool> takesDamageFromWater;
    std::optional<bool> spawnEnabled;
    std::optional<bool> dropsEnabled;
    std::optional<bool> mobGriefing;

    std::optional<std::int32_t> maxNoDamageTicks;
    std::optional<std::int32_t> maxAirSupply;
    std::optional<std::int32_t> breedCooldown;
    std::optional<std::int32_t> despawnSoftRange;
    std::optional<std::int32_t> despawnHardRange;
    std::optional<std::int32_t> explosionRadius;

    friend bool operator==(const MobOverride&, const MobOverride&) = default;
};

/// Every field name the `overrides.<entity_type>` tables accept, in kebab-case.
[[nodiscard]] std::array<std::string_view, 35> mobOverrideFieldNames();

inline constexpr std::size_t kMobCategoryCount = 7;

struct MobCategoryRange {
    std::int32_t softRange = 32;
    std::int32_t hardRange = 128;

    friend bool operator==(const MobCategoryRange&, const MobCategoryRange&) = default;
};

/// Per-world mob, spawning and entity-lifetime settings: vanilla's values until a file says otherwise.
struct MobSettings {
    std::array<std::int32_t, kMobCategoryCount> caps{70, 10, 15, 5, 5, 5, 20};
    std::array<std::int32_t, kMobCategoryCount> spawnIntervalTicks{1, 400, 1, 1, 1, 1, 1};
    std::array<MobCategoryRange, kMobCategoryCount> despawn{
        {{32, 128}, {32, 128}, {32, 128}, {32, 128}, {32, 128}, {32, 128}, {32, 64}}};

    bool perPlayerMobSpawns = false;
    std::int32_t spawnRangeChunks = 8;
    std::int32_t monsterSpawnMaxLightLevel = -1;

    bool despawnCylinder = false;
    std::int32_t despawnIdleTicks = 600;
    std::int32_t despawnRandomOdds = 800;

    std::int32_t itemDespawnTicks = 6000;
    double itemMergeRadius = 0.5;
    double experienceMergeRadius = 0.5;
    std::int32_t arrowDespawnTicks = 1200;
    std::int32_t tridentDespawnTicks = 1200;

    /// Tracking radius in chunks per Spigot category (animals, monsters, misc, display, other); 0 is vanilla's.
    std::array<std::int32_t, 5> trackingRange{0, 0, 0, 0, 0};

    std::int32_t phantomMinSeconds = 60;
    std::int32_t phantomMaxSeconds = 119;
    std::int32_t phantomInsomniaTicks = 72000;

    std::int32_t patrolIntervalTicks = 12000;
    std::int32_t patrolIntervalJitterTicks = 1200;
    std::int32_t patrolAttemptOdds = 5;
    std::int64_t patrolStartTicks = 120000;

    std::int32_t traderSpawnDelayTicks = 24000;
    std::int32_t traderMinChance = 25;
    std::int32_t traderMaxChance = 75;
    std::int32_t traderChanceIncrease = 25;
    std::int32_t traderCheckTicks = 1200;

    std::map<std::string, std::shared_ptr<const MobOverride>, std::less<>> overrides;

    [[nodiscard]] static MobSettings vanilla() { return MobSettings{}; }

    /// Reads one file's tables over `inherited`; `file` only names the file in an error.
    [[nodiscard]] static std::expected<MobSettings, std::string>
    from(const wire::common::toml::TomlTable& table, const MobSettings& inherited, std::string_view file);

    [[nodiscard]] const MobOverride* overrideFor(std::string_view entityTypePath) const;

    friend bool operator==(const MobSettings&, const MobSettings&) = default;
};

/// config/worlds/mobs.toml, generated on first run: the defaults every world inherits.
[[nodiscard]] std::expected<MobSettings, std::string> loadMobDefaults(const ConfigDirectory& directory);

/// worlds/<name>/mobs.toml over the defaults; a broken file costs that world its overrides, logged by name.
[[nodiscard]] MobSettings loadWorldMobs(const std::string& worldDirectory, const MobSettings& defaults);

[[nodiscard]] std::string_view mobCategoryKey(std::size_t index);

} // namespace basalt::server::runtime::config
