#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/server/runtime/config/MobsConfig.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace basalt::server::runtime::config {
namespace {

using wire::common::toml::TomlTable;
using wire::common::toml::TomlValue;

constexpr std::array<std::string_view, kMobCategoryCount> kCategoryKeys{
    "monster", "creature", "ambient", "axolotls", "underground-water-creature", "water-creature", "water-ambient"};

constexpr std::array<std::string_view, 5> kTrackingKeys{"animals", "monsters", "misc", "display", "other"};

template <typename Member>
struct Field {
    std::string_view name;
    Member member;
    double least;
};

using DoubleField = Field<std::optional<double> MobOverride::*>;
using BoolField = Field<std::optional<bool> MobOverride::*>;
using IntField = Field<std::optional<std::int32_t> MobOverride::*>;

constexpr std::array<DoubleField, 16> kDoubleFields{{
    {"max-health", &MobOverride::maxHealth, 0.01},
    {"attack-damage", &MobOverride::attackDamage, 0.0},
    {"movement-speed", &MobOverride::movementSpeed, 0.0},
    {"follow-range", &MobOverride::followRange, 0.0},
    {"armor", &MobOverride::armor, 0.0},
    {"armor-toughness", &MobOverride::armorToughness, 0.0},
    {"knockback-resistance", &MobOverride::knockbackResistance, 0.0},
    {"attack-knockback", &MobOverride::attackKnockback, 0.0},
    {"flying-speed", &MobOverride::flyingSpeed, 0.0},
    {"jump-strength", &MobOverride::jumpStrength, 0.0},
    {"scale", &MobOverride::scale, 0.0625},
    {"safe-fall-distance", &MobOverride::safeFallDistance, 0.0},
    {"fall-damage-multiplier", &MobOverride::fallDamageMultiplier, 0.0},
    {"explosion-knockback-resistance", &MobOverride::explosionKnockbackResistance, 0.0},
    {"oxygen-bonus", &MobOverride::oxygenBonus, 0.0},
    {"experience-multiplier", &MobOverride::experienceMultiplier, 0.0},
}};

constexpr std::array<BoolField, 13> kBoolFields{{
    {"burn-in-daylight", &MobOverride::burnInDaylight, 0.0},
    {"no-ai", &MobOverride::noAi, 0.0},
    {"persistent", &MobOverride::persistent, 0.0},
    {"can-pick-up-loot", &MobOverride::canPickUpLoot, 0.0},
    {"no-gravity", &MobOverride::noGravity, 0.0},
    {"collidable", &MobOverride::collidable, 0.0},
    {"can-swim", &MobOverride::canSwim, 0.0},
    {"opens-doors", &MobOverride::opensDoors, 0.0},
    {"passes-doors", &MobOverride::passesDoors, 0.0},
    {"takes-damage-from-water", &MobOverride::takesDamageFromWater, 0.0},
    {"spawn-enabled", &MobOverride::spawnEnabled, 0.0},
    {"drops-enabled", &MobOverride::dropsEnabled, 0.0},
    {"mob-griefing", &MobOverride::mobGriefing, 0.0},
}};

constexpr std::array<IntField, 6> kIntFields{{
    {"max-no-damage-ticks", &MobOverride::maxNoDamageTicks, 0.0},
    {"max-air-supply", &MobOverride::maxAirSupply, 0.0},
    {"breed-cooldown", &MobOverride::breedCooldown, 0.0},
    {"despawn-soft-range", &MobOverride::despawnSoftRange, 0.0},
    {"despawn-hard-range", &MobOverride::despawnHardRange, 0.0},
    {"explosion-radius", &MobOverride::explosionRadius, 0.0},
}};

std::string kebab(std::string_view key) {
    std::string spelled{key};
    std::ranges::replace(spelled, '_', '-');
    return spelled;
}

[[nodiscard]] std::string where(std::string_view file, std::string_view section, std::string_view key) {
    return std::string{file} + ": " + std::string{section} + "." + std::string{key};
}

struct Reader {
    const TomlTable& table;
    std::string_view file;
    std::string section;
    std::optional<std::string> error;

    [[nodiscard]] const TomlValue* find(std::string_view key) const { return table.find(key); }

    void fail(std::string_view key, std::string_view expected) {
        if (!error.has_value()) {
            error = where(file, section, key) + " must be " + std::string{expected} + ".";
        }
    }

    void integer(std::string_view key, std::int32_t& into, std::int64_t least, std::int64_t most = 1'000'000'000) {
        if (find(key) == nullptr) {
            return;
        }
        const std::optional<std::int64_t> value = table.integer(key);
        if (!value.has_value() || *value < least || *value > most) {
            fail(key, "a whole number from " + std::to_string(least) + " to " + std::to_string(most));
            return;
        }
        into = static_cast<std::int32_t>(*value);
    }

    void real(std::string_view key, double& into, double least) {
        if (find(key) == nullptr) {
            return;
        }
        const std::optional<double> value = table.real(key);
        if (!value.has_value() || !std::isfinite(*value) || *value < least) {
            fail(key, "a number of at least " + std::to_string(least));
            return;
        }
        into = *value;
    }

    void boolean(std::string_view key, bool& into) {
        if (find(key) == nullptr) {
            return;
        }
        const std::optional<bool> value = table.boolean(key);
        if (!value.has_value()) {
            fail(key, "true or false");
            return;
        }
        into = *value;
    }
};

void readOverrideFields(const TomlTable& table, MobOverride& into, std::string_view file, const std::string& type,
                        std::optional<std::string>& error) {
    for (const std::string& key : table.keys()) {
        const std::string name = kebab(key);
        const auto known = [&name](const auto& fields) {
            return std::ranges::find(fields, name, &std::remove_cvref_t<decltype(fields[0])>::name) != fields.end();
        };
        if (!known(kDoubleFields) && !known(kBoolFields) && !known(kIntFields)) {
            error = std::string{file} + ": overrides." + type + "." + key +
                    " is not a known field. Known fields are listed in the file's reference.";
            return;
        }
    }
    const auto lookup = [&table](const std::string_view name) -> std::pair<const TomlValue*, std::string> {
        if (const TomlValue* found = table.find(name)) {
            return {found, std::string{name}};
        }
        std::string snake{name};
        std::ranges::replace(snake, '-', '_');
        return {table.find(snake), snake};
    };
    for (const DoubleField& field : kDoubleFields) {
        const auto [value, spelled] = lookup(field.name);
        if (value == nullptr) {
            continue;
        }
        const std::optional<double> number = table.real(spelled);
        if (!number.has_value() || !std::isfinite(*number) || *number < field.least) {
            error = std::string{file} + ": overrides." + type + "." + spelled + " must be a number of at least " +
                    std::to_string(field.least) + ".";
            return;
        }
        into.*(field.member) = *number;
    }
    for (const BoolField& field : kBoolFields) {
        const auto [value, spelled] = lookup(field.name);
        if (value == nullptr) {
            continue;
        }
        const std::optional<bool> flag = table.boolean(spelled);
        if (!flag.has_value()) {
            error = std::string{file} + ": overrides." + type + "." + spelled + " must be true or false.";
            return;
        }
        into.*(field.member) = *flag;
    }
    for (const IntField& field : kIntFields) {
        const auto [value, spelled] = lookup(field.name);
        if (value == nullptr) {
            continue;
        }
        const std::optional<std::int64_t> number = table.integer(spelled);
        if (!number.has_value() || *number < 0 || *number > std::numeric_limits<std::int32_t>::max()) {
            error =
                std::string{file} + ": overrides." + type + "." + spelled + " must be a whole number of at least 0.";
            return;
        }
        into.*(field.member) = static_cast<std::int32_t>(*number);
    }
}

[[nodiscard]] std::optional<std::string> readOverrides(const TomlTable& overrides, MobSettings& settings,
                                                       std::string_view file) {
    for (const std::string& rawKey : overrides.keys()) {
        const TomlTable* table = overrides.table(rawKey);
        if (table == nullptr) {
            return std::string{file} + ": overrides." + rawKey + " must be a table.";
        }
        const std::string_view path = rawKey.starts_with("minecraft:") ? std::string_view{rawKey}.substr(10) : rawKey;
        if (registry::entityTypes::of(wire::common::Identifier::vanilla(std::string{path})) == nullptr) {
            return std::string{file} + ": overrides." + rawKey + " is not an entity type.";
        }
        MobOverride merged;
        if (const auto inherited = settings.overrides.find(path); inherited != settings.overrides.end()) {
            merged = *inherited->second;
        }
        std::optional<std::string> error;
        readOverrideFields(*table, merged, file, rawKey, error);
        if (error.has_value()) {
            return error;
        }
        settings.overrides[std::string{path}] = std::make_shared<const MobOverride>(merged);
    }
    return std::nullopt;
}

} // namespace

std::array<std::string_view, 35> mobOverrideFieldNames() {
    std::array<std::string_view, 35> names{};
    std::size_t at = 0;
    for (const DoubleField& field : kDoubleFields) {
        names[at++] = field.name;
    }
    for (const BoolField& field : kBoolFields) {
        names[at++] = field.name;
    }
    for (const IntField& field : kIntFields) {
        names[at++] = field.name;
    }
    return names;
}

std::string_view mobCategoryKey(std::size_t index) { return index < kCategoryKeys.size() ? kCategoryKeys[index] : ""; }

const MobOverride* MobSettings::overrideFor(std::string_view entityTypePath) const {
    const auto found = overrides.find(entityTypePath);
    return found == overrides.end() ? nullptr : found->second.get();
}

std::expected<MobSettings, std::string> MobSettings::from(const TomlTable& table, const MobSettings& inherited,
                                                          std::string_view file) {
    MobSettings settings = inherited;
    std::optional<std::string> error;
    const auto read = [&](std::string_view name, auto&& body) {
        if (error.has_value()) {
            return;
        }
        const TomlTable* found = table.table(name);
        if (found == nullptr) {
            return;
        }
        Reader reader{*found, file, std::string{name}, std::nullopt};
        body(reader);
        error = reader.error;
    };

    read("caps", [&](Reader& r) {
        for (std::size_t at = 0; at < kMobCategoryCount; ++at) {
            r.integer(kCategoryKeys[at], settings.caps[at], 0);
        }
    });
    read("spawn-interval-ticks", [&](Reader& r) {
        for (std::size_t at = 0; at < kMobCategoryCount; ++at) {
            r.integer(kCategoryKeys[at], settings.spawnIntervalTicks[at], 0);
        }
    });
    read("spawning", [&](Reader& r) {
        r.boolean("per-player-mob-spawns", settings.perPlayerMobSpawns);
        r.integer("range-chunks", settings.spawnRangeChunks, 1, 32);
        r.integer("monster-max-light-level", settings.monsterSpawnMaxLightLevel, -1, 15);
    });
    read("despawn", [&](Reader& r) {
        if (r.find("shape") != nullptr) {
            const std::optional<std::string_view> name = r.table.string("shape");
            if (!name.has_value() || (*name != "sphere" && *name != "cylinder")) {
                r.fail("shape", "\"sphere\" or \"cylinder\"");
            } else {
                settings.despawnCylinder = *name == "cylinder";
            }
        }
        r.integer("idle-ticks-before-random", settings.despawnIdleTicks, 0);
        r.integer("random-odds", settings.despawnRandomOdds, 1);
        for (std::size_t at = 0; at < kMobCategoryCount && !r.error.has_value(); ++at) {
            const TomlTable* category = r.table.table(kCategoryKeys[at]);
            if (category == nullptr) {
                continue;
            }
            Reader inner{*category, file, "despawn." + std::string{kCategoryKeys[at]}, std::nullopt};
            inner.integer("soft-range", settings.despawn[at].softRange, 0, 10'000);
            inner.integer("hard-range", settings.despawn[at].hardRange, 0, 10'000);
            r.error = inner.error;
        }
    });
    read("items", [&](Reader& r) {
        r.integer("despawn-ticks", settings.itemDespawnTicks, 1);
        r.real("merge-radius", settings.itemMergeRadius, 0.0);
    });
    read("experience", [&](Reader& r) { r.real("merge-radius", settings.experienceMergeRadius, 0.0); });
    read("projectiles", [&](Reader& r) {
        r.integer("arrow-despawn-ticks", settings.arrowDespawnTicks, 1);
        r.integer("trident-despawn-ticks", settings.tridentDespawnTicks, 1);
    });
    read("tracking-range", [&](Reader& r) {
        for (std::size_t at = 0; at < kTrackingKeys.size(); ++at) {
            r.integer(kTrackingKeys[at], settings.trackingRange[at], 0, 128);
        }
    });
    read("phantoms", [&](Reader& r) {
        r.integer("spawn-attempt-min-seconds", settings.phantomMinSeconds, 1);
        r.integer("spawn-attempt-max-seconds", settings.phantomMaxSeconds, 1);
        r.integer("insomnia-ticks", settings.phantomInsomniaTicks, 1);
    });
    read("pillager-patrols", [&](Reader& r) {
        r.integer("interval-ticks", settings.patrolIntervalTicks, 1);
        r.integer("interval-jitter-ticks", settings.patrolIntervalJitterTicks, 1);
        r.integer("attempt-odds", settings.patrolAttemptOdds, 1);
        std::int32_t start = static_cast<std::int32_t>(settings.patrolStartTicks);
        r.integer("start-ticks", start, 0);
        settings.patrolStartTicks = start;
    });
    read("wandering-trader", [&](Reader& r) {
        r.integer("spawn-delay-ticks", settings.traderSpawnDelayTicks, 1);
        r.integer("min-spawn-chance", settings.traderMinChance, 0, 100);
        r.integer("max-spawn-chance", settings.traderMaxChance, 0, 100);
        r.integer("spawn-chance-increase", settings.traderChanceIncrease, 0, 100);
        r.integer("check-interval-ticks", settings.traderCheckTicks, 1);
    });
    if (!error.has_value()) {
        if (const TomlTable* overrides = table.table("overrides")) {
            error = readOverrides(*overrides, settings, file);
        }
    }
    settings.phantomMaxSeconds = std::max(settings.phantomMaxSeconds, settings.phantomMinSeconds);
    settings.traderMaxChance = std::max(settings.traderMaxChance, settings.traderMinChance);
    if (error.has_value()) {
        return std::unexpected(*error);
    }
    return settings;
}

std::expected<MobSettings, std::string> loadMobDefaults(const ConfigDirectory& directory) {
    const ConfigDirectory worlds{directory.root() + "/worlds"};
    const std::expected<TomlTable, std::string> root = worlds.load("mobs.toml", kWorldMobsDefaultToml);
    if (!root.has_value()) {
        return std::unexpected(root.error());
    }
    return MobSettings::from(*root, MobSettings::vanilla(), worlds.pathOf("mobs.toml"));
}

MobSettings loadWorldMobs(const std::string& worldDirectory, const MobSettings& defaults) {
    const std::string path = worldDirectory + "/mobs.toml";
    const std::optional<std::string> text = wire::io::fileIO::readText(path);
    if (!text.has_value()) {
        if (wire::io::fileIO::isDirectory(worldDirectory)) {
            wire::io::fileIO::writeText(path, kWorldMobsLocalDefaultToml);
        }
        return defaults;
    }
    const wire::common::Logger logger{"Config"};
    const std::expected<TomlTable, wire::common::toml::TomlError> parsed = wire::common::toml::parse(*text);
    if (!parsed.has_value()) {
        logger.warn([&path] { return path + " is not valid TOML; this world keeps the default mob settings."; });
        return defaults;
    }
    std::expected<MobSettings, std::string> settings = MobSettings::from(*parsed, defaults, path);
    if (!settings.has_value()) {
        logger.warn([&settings] { return settings.error() + " This world keeps the default mob settings."; });
        return defaults;
    }
    return *std::move(settings);
}

} // namespace basalt::server::runtime::config
