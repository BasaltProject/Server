#include <basalt/server/runtime/world/MobEffects.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world::mobEffects {
namespace {

/// The registry in its own order, so the index is the protocol id.
constexpr auto kNames =
    std::to_array<std::string_view>({"speed",           "slowness",       "haste",          "mining_fatigue",
                                     "strength",        "instant_health", "instant_damage", "jump_boost",
                                     "nausea",          "regeneration",   "resistance",     "fire_resistance",
                                     "water_breathing", "invisibility",   "blindness",      "night_vision",
                                     "hunger",          "weakness",       "poison",         "wither",
                                     "health_boost",    "absorption",     "saturation",     "glowing",
                                     "levitation",      "luck",           "unluck",         "slow_falling",
                                     "conduit_power",   "dolphins_grace", "bad_omen",       "hero_of_the_village",
                                     "darkness",        "trial_omen",     "raid_omen",      "wind_charged",
                                     "weaving",         "oozing",         "infested",       "breath_of_the_nautilus"});

/// One instance per name, built once, so a caller may hold on to a reference.
[[nodiscard]] const std::vector<wire::common::Identifier>& instances() {
    static const std::vector<wire::common::Identifier> built = [] {
        std::vector<wire::common::Identifier> all;
        all.reserve(kNames.size());
        for (const std::string_view name : kNames) {
            all.push_back(wire::common::Identifier::vanilla(std::string{name}));
        }
        return all;
    }();
    return built;
}

[[nodiscard]] const wire::common::Identifier& named(std::string_view path) {
    for (std::size_t index = 0; index < kNames.size(); ++index) {
        if (kNames[index] == path) {
            return instances()[index];
        }
    }
    return instances()[0];
}

} // namespace

std::optional<std::int32_t> protocolIdOf(const wire::common::Identifier& effect) {
    if (effect.namespaceName() != "minecraft") {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < kNames.size(); ++index) {
        if (kNames[index] == effect.path()) {
            return static_cast<std::int32_t>(index);
        }
    }
    return std::nullopt;
}

std::optional<wire::common::Identifier> byProtocolId(std::int32_t id) {
    if (id < 0 || id >= static_cast<std::int32_t>(kNames.size())) {
        return std::nullopt;
    }
    return named(kNames[static_cast<std::size_t>(id)]);
}

std::vector<wire::common::Identifier> ids() { return instances(); }

const wire::common::Identifier& regeneration() { return named("regeneration"); }

const wire::common::Identifier& poison() { return named("poison"); }

const wire::common::Identifier& wither() { return named("wither"); }

const wire::common::Identifier& hunger() { return named("hunger"); }

const wire::common::Identifier& instantHealth() { return named("instant_health"); }

const wire::common::Identifier& instantDamage() { return named("instant_damage"); }

const wire::common::Identifier& haste() { return named("haste"); }

const wire::common::Identifier& miningFatigue() { return named("mining_fatigue"); }

const wire::common::Identifier& badOmen() { return named("bad_omen"); }

const wire::common::Identifier& raidOmen() { return named("raid_omen"); }

const wire::common::Identifier& heroOfTheVillage() { return named("hero_of_the_village"); }

const wire::common::Identifier& resistance() { return named("resistance"); }

const wire::common::Identifier& absorption() { return named("absorption"); }

} // namespace basalt::server::runtime::world::mobEffects
