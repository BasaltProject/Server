#include <basalt/server/runtime/world/MobEffects.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world::mobEffects {
namespace {

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

/// parity: the colour each `MobEffects` entry is registered with, in the same order.
constexpr std::array<std::int32_t, 40> kColors{
    3402751,  9154528,  14270531, 4866583,  16762624, 16262179, 11101546, 16646020, 5578058, 13458603,
    9520880,  16750848, 10017472, 16185078, 2039587,  12779366, 5797459,  4738376,  8889187, 7561558,
    16284963, 2445989,  16262179, 9740385,  13565951, 5882118,  12624973, 15978425, 1950417, 8954814,
    745784,   4521796,  2696993,  1484454,  14565464, 12438015, 7891290,  10092451, 9214860, 65518};

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

const wire::common::Identifier& darkness() { return named("darkness"); }

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

std::optional<std::int32_t> colorOf(const wire::common::Identifier& effect) {
    const std::optional<std::int32_t> id = protocolIdOf(effect);
    if (!id.has_value() || *id < 0 || static_cast<std::size_t>(*id) >= kColors.size()) {
        return std::nullopt;
    }
    return kColors[static_cast<std::size_t>(*id)];
}

std::optional<std::string_view> ownParticleOf(const wire::common::Identifier& effect) {
    if (effect.path() == "trial_omen") {
        return "trial_omen";
    }
    if (effect.path() == "raid_omen") {
        return "raid_omen";
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::world::mobEffects
