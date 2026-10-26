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

constexpr std::array<DoubleField, 29> kDoubleFields{{
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
    {"gravity", &MobOverride::gravity, -1.0},
    {"step-height", &MobOverride::stepHeight, 0.0},
    {"tempt-range", &MobOverride::temptRange, 0.0},
    {"baby-chance", &MobOverride::babyChance, 0.0},
    {"jockey-chance", &MobOverride::jockeyChance, 0.0},
    {"charged-chance", &MobOverride::chargedChance, 0.0},
    {"offspring-chance", &MobOverride::offspringChance, 0.0},
    {"max-health-min", &MobOverride::maxHealthMin, 0.01},
    {"max-health-max", &MobOverride::maxHealthMax, 0.01},
    {"movement-speed-min", &MobOverride::movementSpeedMin, 0.0},
    {"movement-speed-max", &MobOverride::movementSpeedMax, 0.0},
    {"jump-strength-min", &MobOverride::jumpStrengthMin, 0.0},
    {"jump-strength-max", &MobOverride::jumpStrengthMax, 0.0},
}};

constexpr std::array<BoolField, 22> kBoolFields{{
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
    {"ridable", &MobOverride::ridable, 0.0},
    {"controllable", &MobOverride::controllable, 0.0},
    {"ridable-in-water", &MobOverride::ridableInWater, 0.0},
    {"always-drop-exp", &MobOverride::alwaysDropExp, 0.0},
    {"can-breed", &MobOverride::canBreed, 0.0},
    {"explode-when-killed", &MobOverride::explodeWhenKilled, 0.0},
    {"jockey-only-babies", &MobOverride::jockeyOnlyBabies, 0.0},
    {"jockey-try-existing-chickens", &MobOverride::jockeyTryExistingChickens, 0.0},
    {"drops-replace", &MobOverride::dropsReplace, 0.0},
}};

constexpr std::array<IntField, 14> kIntFields{{
    {"max-no-damage-ticks", &MobOverride::maxNoDamageTicks, 0.0},
    {"max-air-supply", &MobOverride::maxAirSupply, 0.0},
    {"breed-cooldown", &MobOverride::breedCooldown, 0.0},
    {"despawn-soft-range", &MobOverride::despawnSoftRange, 0.0},
    {"despawn-hard-range", &MobOverride::despawnHardRange, 0.0},
    {"explosion-radius", &MobOverride::explosionRadius, 0.0},
    {"ridable-max-y", &MobOverride::ridableMaxY, -2048.0},
    {"offspring-min", &MobOverride::offspringMin, 0.0},
    {"offspring-max", &MobOverride::offspringMax, 0.0},
    {"spawn-weight", &MobOverride::spawnWeight, 0.0},
    {"spawn-min-light", &MobOverride::spawnMinLight, 0.0},
    {"spawn-max-light", &MobOverride::spawnMaxLight, 0.0},
    {"spawn-min-y", &MobOverride::spawnMinY, -2048.0},
    {"spawn-max-y", &MobOverride::spawnMaxY, -2048.0},
}};

constexpr std::array<std::string_view, 106> kGoalNames{"anger_target",
                                                       "angry_at_player",
                                                       "avoid_entity",
                                                       "bat_flight",
                                                       "beam_attack",
                                                       "become_angry_target",
                                                       "bee_attack",
                                                       "beg",
                                                       "blaze_attack",
                                                       "breath_air",
                                                       "breed",
                                                       "cast_spell",
                                                       "cat_lie_on_block",
                                                       "cat_relax_on_owner",
                                                       "cat_sit_on_block",
                                                       "climb_walls",
                                                       "copper_golem_idle",
                                                       "copper_golem_transport",
                                                       "crossbow_attack",
                                                       "dolphin_jump",
                                                       "dolphin_move_to_item",
                                                       "dolphin_play_with_items",
                                                       "dolphin_swim_to_treasure",
                                                       "dolphin_swim_with_player",
                                                       "eat_block",
                                                       "enderman_freeze_when_looked_at",
                                                       "enderman_look_for_player",
                                                       "enter_hive",
                                                       "fireball_attack",
                                                       "float",
                                                       "follow_flock_leader",
                                                       "follow_owner",
                                                       "follow_parent",
                                                       "follow_ridden_boat",
                                                       "ghast_float",
                                                       "ghast_look",
                                                       "go_to_hive",
                                                       "go_to_known_flower",
                                                       "grow_crop",
                                                       "hop_about",
                                                       "hover_wander",
                                                       "hurt_by_other",
                                                       "hurt_by_target",
                                                       "johnny_attack",
                                                       "land_on_owners_shoulder",
                                                       "leap_at_target",
                                                       "llama_attack_wolf",
                                                       "llama_follow_caravan",
                                                       "llama_hurt_by_target",
                                                       "llama_spit",
                                                       "locate_hive",
                                                       "long_distance_patrol",
                                                       "look_at_player",
                                                       "look_at_player_sometimes",
                                                       "melee_attack",
                                                       "move_towards_trader_home",
                                                       "nearest_attackable_entity",
                                                       "nearest_attackable_player",
                                                       "obtain_raid_leader_banner",
                                                       "owner_hurt_by_target",
                                                       "owner_hurt_target",
                                                       "panic",
                                                       "pathfind_to_raid",
                                                       "phantom_circle",
                                                       "phantom_strategy",
                                                       "phantom_sweep",
                                                       "phantom_target",
                                                       "pollinate",
                                                       "raider_celebration",
                                                       "raider_move_through_village",
                                                       "random_look_around",
                                                       "random_stand",
                                                       "random_stroll",
                                                       "random_swimming",
                                                       "ranged_attack",
                                                       "reset_universal_anger_target",
                                                       "run_around_like_crazy",
                                                       "shulker_attack",
                                                       "shulker_peek",
                                                       "shulker_target",
                                                       "sit_when_ordered_to",
                                                       "swell",
                                                       "swim_about",
                                                       "teleport_away",
                                                       "tempt",
                                                       "throw_potion",
                                                       "trader_llama_defend_trader",
                                                       "trader_use_item",
                                                       "try_find_water",
                                                       "turtle_go_home",
                                                       "turtle_go_to_water",
                                                       "turtle_lay_egg",
                                                       "turtle_random_stroll",
                                                       "turtle_travel",
                                                       "validate",
                                                       "vex_charge",
                                                       "vex_random_move",
                                                       "wander",
                                                       "wandering_trader_wander",
                                                       "warden_busy",
                                                       "warden_investigate",
                                                       "warden_sniff",
                                                       "wither_do_nothing",
                                                       "wither_target",
                                                       "wolf_avoid_llama",
                                                       "wololo_spell"};

constexpr std::array<std::string_view, 4> kListFields{"drops", "breeding-items", "disabled-goals", "spawn-biomes"};

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

[[nodiscard]] std::optional<std::string_view> findKey(const TomlTable& table, std::string_view name) {
    for (const std::string& key : table.keys()) {
        if (kebab(key) == name) {
            return std::string_view{key};
        }
    }
    return std::nullopt;
}

void readLists(const TomlTable& table, MobOverride& into, std::string_view file, const std::string& type,
               std::optional<std::string>& error) {
    const auto fail = [&](std::string_view key, std::string_view what) {
        error = std::string{file} + ": overrides." + type + "." + std::string{key} + " " + std::string{what};
    };
    if (const auto key = findKey(table, "drops")) {
        const TomlTable* drops = table.table(*key);
        if (drops == nullptr) {
            return fail("drops", "must be a table of item tables, for example [overrides.cow.drops.diamond].");
        }
        into.drops.clear();
        for (const std::string& itemKey : drops->keys()) {
            const TomlTable* entry = drops->table(itemKey);
            const std::string itemName = itemKey.find(':') == std::string::npos ? "minecraft:" + itemKey : itemKey;
            const auto parsed = wire::common::Identifier::parse(itemName);
            if (entry == nullptr || !parsed.has_value() || registry::items::registry().byName(*parsed) == nullptr) {
                return fail("drops." + itemKey, "must be a table naming an item, with chance, min and max.");
            }
            MobDrop drop{.item = parsed->toString()};
            Reader reader{*entry, file, "overrides." + type + ".drops." + itemKey, std::nullopt};
            reader.real("chance", drop.chance, 0.0);
            reader.integer("min", drop.minCount, 0, 64);
            reader.integer("max", drop.maxCount, 0, 64);
            if (reader.error.has_value()) {
                error = reader.error;
                return;
            }
            if (drop.chance > 1.0 || drop.maxCount < drop.minCount) {
                return fail("drops." + itemKey, "needs a chance of at most 1 and a max of at least min.");
            }
            into.drops.push_back(std::move(drop));
        }
    }
    const auto strings = [&](std::string_view name, std::optional<std::vector<std::string>>& target, auto&& valid,
                             std::string_view what) {
        const auto key = findKey(table, name);
        if (!key.has_value() || error.has_value()) {
            return;
        }
        std::vector<std::string> list = table.stringList(*key);
        const TomlValue* raw = table.find(*key);
        const auto* array = raw == nullptr ? nullptr : raw->get<std::vector<TomlValue>>();
        if (array == nullptr || array->size() != list.size()) {
            return fail(name, "must be a list of strings.");
        }
        for (std::string& entry : list) {
            if (!valid(entry)) {
                return fail(name, std::string{what} + " Found \"" + entry + "\".");
            }
        }
        target = std::move(list);
    };
    strings(
        "breeding-items", into.breedingItems,
        [](std::string& name) {
            if (name.find(':') == std::string::npos) {
                name = "minecraft:" + name;
            }
            const auto parsed = wire::common::Identifier::parse(name);
            return parsed.has_value() && registry::items::registry().byName(*parsed) != nullptr;
        },
        "must hold item names.");
    strings(
        "disabled-goals", into.disabledGoals,
        [](std::string& name) { return std::ranges::find(kGoalNames, name) != kGoalNames.end(); },
        "must hold goal names, such as panic or look_at_player.");
    strings(
        "spawn-biomes", into.spawnBiomes,
        [](std::string& name) {
            const bool tag = name.starts_with('#');
            std::string bare = tag ? name.substr(1) : name;
            if (bare.find(':') == std::string::npos) {
                bare = "minecraft:" + bare;
            }
            name = (tag ? "#" : "") + bare;
            return wire::common::Identifier::parse(bare).has_value();
        },
        "must hold biome names or #tags.");
}

void readOverrideFields(const TomlTable& table, MobOverride& into, std::string_view file, const std::string& type,
                        std::optional<std::string>& error) {
    for (const std::string& key : table.keys()) {
        const std::string name = kebab(key);
        const auto known = [&name](const auto& fields) {
            return std::ranges::find(fields, name, &std::remove_cvref_t<decltype(fields[0])>::name) != fields.end();
        };
        if (!known(kDoubleFields) && !known(kBoolFields) && !known(kIntFields) &&
            std::ranges::find(kListFields, name) == kListFields.end()) {
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
    readLists(table, into, file, type, error);
    if (!error.has_value() && into.offspringMin.has_value() && into.offspringMax.has_value() &&
        *into.offspringMax < *into.offspringMin) {
        error = std::string{file} + ": overrides." + type + ".offspring-max must not be below offspring-min.";
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

std::vector<std::string_view> mobOverrideFieldNames() {
    std::vector<std::string_view> names;
    for (const DoubleField& field : kDoubleFields) {
        names.push_back(field.name);
    }
    for (const BoolField& field : kBoolFields) {
        names.push_back(field.name);
    }
    for (const IntField& field : kIntFields) {
        names.push_back(field.name);
    }
    names.insert(names.end(), kListFields.begin(), kListFields.end());
    return names;
}

std::span<const std::string_view> knownGoalNames() { return kGoalNames; }

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
