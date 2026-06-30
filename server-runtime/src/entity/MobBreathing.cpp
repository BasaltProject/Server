#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string>

namespace basalt::server::runtime::entity::mobBreathing {
namespace {

using namespace std::string_view_literals;

/// parity: `shouldTakeDrowningDamage`, twenty ticks past empty.
constexpr std::int32_t kDrowningAir = -20;
constexpr float kDrownDamage = 2.0F;
constexpr std::int32_t kAxolotlMaxAir = 6000;
/// parity: `EntityDimensions.scalable`.
constexpr float kEyeShare = 0.85F;
/// parity: `LivingEntity.SLEEPING_DIMENSIONS`.
constexpr float kSleepingEye = 0.2F;

struct Eye {
    std::string_view type;
    float height;
};

/// parity: every `EntityType.Builder.eyeHeight` of a living thing in `EntityTypes`.
constexpr std::array kEyes{Eye{"allay", 0.36F},
                           Eye{"armadillo", 0.26F},
                           Eye{"armor_stand", 1.7775F},
                           Eye{"axolotl", 0.2751F},
                           Eye{"bat", 0.45F},
                           Eye{"bee", 0.3F},
                           Eye{"bogged", 1.74F},
                           Eye{"breeze", 1.3452F},
                           Eye{"camel", 2.275F},
                           Eye{"camel_husk", 2.275F},
                           Eye{"cat", 0.35F},
                           Eye{"cave_spider", 0.45F},
                           Eye{"chicken", 0.644F},
                           Eye{"cod", 0.195F},
                           Eye{"copper_golem", 0.8125F},
                           Eye{"cow", 1.3F},
                           Eye{"creaking", 2.3F},
                           Eye{"dolphin", 0.3F},
                           Eye{"donkey", 1.425F},
                           Eye{"drowned", 1.74F},
                           Eye{"elder_guardian", 0.99875F},
                           Eye{"enderman", 2.55F},
                           Eye{"endermite", 0.13F},
                           Eye{"fox", 0.4F},
                           Eye{"ghast", 2.6F},
                           Eye{"happy_ghast", 2.6F},
                           Eye{"giant", 10.44F},
                           Eye{"glow_squid", 0.4F},
                           Eye{"guardian", 0.425F},
                           Eye{"horse", 1.52F},
                           Eye{"husk", 1.74F},
                           Eye{"llama", 1.7765F},
                           Eye{"magma_cube", 0.325F},
                           Eye{"mannequin", 1.62F},
                           Eye{"mooshroom", 1.3F},
                           Eye{"mule", 1.52F},
                           Eye{"nautilus", 0.2751F},
                           Eye{"parched", 1.74F},
                           Eye{"parrot", 0.54F},
                           Eye{"phantom", 0.175F},
                           Eye{"piglin", 1.79F},
                           Eye{"piglin_brute", 1.79F},
                           Eye{"pufferfish", 0.455F},
                           Eye{"rabbit", 0.59F},
                           Eye{"salmon", 0.26F},
                           Eye{"sheep", 1.235F},
                           Eye{"shulker", 0.5F},
                           Eye{"silverfish", 0.13F},
                           Eye{"skeleton", 1.74F},
                           Eye{"skeleton_horse", 1.52F},
                           Eye{"slime", 0.325F},
                           Eye{"sniffer", 1.05F},
                           Eye{"snow_golem", 1.7F},
                           Eye{"spider", 0.65F},
                           Eye{"squid", 0.4F},
                           Eye{"stray", 1.74F},
                           Eye{"sulfur_cube", 0.175F},
                           Eye{"tadpole", 0.19500001F},
                           Eye{"trader_llama", 1.7765F},
                           Eye{"tropical_fish", 0.26F},
                           Eye{"vex", 0.51875F},
                           Eye{"villager", 1.62F},
                           Eye{"wandering_trader", 1.62F},
                           Eye{"witch", 1.62F},
                           Eye{"wither_skeleton", 2.1F},
                           Eye{"wolf", 0.68F},
                           Eye{"zombie", 1.74F},
                           Eye{"zombie_horse", 1.52F},
                           Eye{"zombie_nautilus", 0.2751F},
                           Eye{"zombie_villager", 1.74F},
                           Eye{"zombified_piglin", 1.79F}};

/// parity: the `BABY_DIMENSIONS` that name their own eye rather than scaling the grown one.
constexpr std::array kBabyEyes{
    Eye{"villager", 0.63F},       Eye{"strider", 0.4375F},     Eye{"zombie_villager", 0.67F},
    Eye{"hoglin", 0.625F},        Eye{"zoglin", 0.625F},       Eye{"zombified_piglin", 0.78F},
    Eye{"piglin", 0.78F},         Eye{"drowned", 0.775F},      Eye{"zombie", 0.775F},
    Eye{"husk", 0.825F},          Eye{"polar_bear", 0.34375F}, Eye{"armadillo", 0.21875F},
    Eye{"dolphin", 0.09375F},     Eye{"panda", 0.28125F},      Eye{"cow", 0.69F},
    Eye{"mooshroom", 0.69F},      Eye{"ocelot", 0.34375F},     Eye{"goat", 0.59375F},
    Eye{"axolotl", 0.09375F},     Eye{"fox", 0.34375F},        Eye{"squid", 0.37F},
    Eye{"glow_squid", 0.37F},     Eye{"pig", 0.40625F},        Eye{"wolf", 0.34375F},
    Eye{"happy_ghast", 0.46875F}, Eye{"rabbit", 0.39F},        Eye{"sheep", 0.65625F},
    Eye{"chicken", 0.28125F},     Eye{"cat", 0.34375F},        Eye{"armor_stand", 0.9875F}};

/// parity: the grown eye scaled by the baby's own `BABY_DIMENSIONS` or `getAgeScale`.
[[nodiscard]] float babyScaleOf(std::string_view path) {
    if (path == "turtle") {
        return 0.3F;
    }
    if (path == "horse" || path == "zombie_horse" || path == "skeleton_horse") {
        return 0.7F;
    }
    return 0.5F;
}

[[nodiscard]] std::optional<float> eyeIn(std::span<const Eye> table, std::string_view path) {
    const auto found = std::ranges::find(table, path, &Eye::type);
    return found == table.end() ? std::nullopt : std::optional<float>{found->height};
}

/// parity: `#minecraft:can_breathe_under_water`, with `#undead` spelt out.
constexpr std::array kBreathers{
    "skeleton"sv,     "stray"sv,      "wither_skeleton"sv, "skeleton_horse"sv,  "bogged"sv,           "parched"sv,
    "zombie_horse"sv, "camel_husk"sv, "zombie"sv,          "zombie_villager"sv, "zombified_piglin"sv, "zoglin"sv,
    "drowned"sv,      "husk"sv,       "zombie_nautilus"sv, "wither"sv,          "phantom"sv,          "axolotl"sv,
    "frog"sv,         "guardian"sv,   "elder_guardian"sv,  "turtle"sv,          "glow_squid"sv,       "cod"sv,
    "pufferfish"sv,   "salmon"sv,     "squid"sv,           "tropical_fish"sv,   "tadpole"sv,          "armor_stand"sv,
    "copper_golem"sv, "nautilus"sv};

/// parity: `WaterAnimal` and `AgeableWaterCreature`, the two that choke on land.
constexpr std::array kWaterAnimals{"cod"sv,     "salmon"sv, "tropical_fish"sv, "pufferfish"sv,
                                   "tadpole"sv, "squid"sv,  "glow_squid"sv,    "dolphin"sv};

enum class Gasp : std::uint8_t { None, WaterAnimal, Axolotl, Nautilus };

[[nodiscard]] Gasp gaspOf(std::string_view path) {
    // parity: the dolphin empties its own `handleAirSupply`.
    if (path == "dolphin") {
        return Gasp::None;
    }
    if (std::ranges::find(kWaterAnimals, path) != kWaterAnimals.end()) {
        return Gasp::WaterAnimal;
    }
    if (path == "axolotl") {
        return Gasp::Axolotl;
    }
    return path == "nautilus" ? Gasp::Nautilus : Gasp::None;
}

} // namespace

std::int32_t maxAirOf(std::string_view typePath) {
    if (typePath == "dolphin") {
        return ServerEntity::kDolphinMaxAir;
    }
    return typePath == "axolotl" ? kAxolotlMaxAir : ServerEntity::kDefaultMaxAir;
}

bool breathesUnderwater(std::string_view typePath) {
    return std::ranges::find(kBreathers, typePath) != kBreathers.end();
}

double eyeHeightOf(const ServerEntity& mob) {
    if (mob.sleeping) {
        return static_cast<double>(kSleepingEye);
    }
    const std::string_view path = mob.type().path();
    float eye = eyeIn(kEyes, path).value_or(static_cast<float>(mob.height()) * kEyeShare);
    const bool baby = mob.age < 0 || (mob.sourceNbt.has_value() && mob.sourceNbt->getBoolean("IsBaby"));
    if (baby) {
        const std::optional<float> own = eyeIn(kBabyEyes, path);
        eye = own.value_or(eye * babyScaleOf(path));
    }
    // parity: `AbstractCubeMob.getDefaultDimensions` scales by the size.
    if (path == "slime" || path == "magma_cube" || path == "sulfur_cube") {
        eye *= static_cast<float>(mob.cubeSize);
    } else if (path == "phantom") {
        const std::int32_t size = mob.sourceNbt.has_value() ? entityNbt::numericInt(*mob.sourceNbt, "size") : 0;
        eye *= 1.0F + (0.15F * static_cast<float>(size));
    }
    return static_cast<double>(eye * static_cast<float>(mob.attributeWithModifiers("minecraft:scale")));
}

std::vector<Hurt> tick(ServerEntity& mob, const Surroundings& at) {
    const std::string_view path = mob.type().path();
    const std::int32_t maxAir = mob.maxAirOverride.value_or(maxAirOf(path));
    const std::int32_t before = mob.airSupply;
    std::vector<Hurt> hurts;
    // parity: `increaseAirSupply`, four a tick, and all of it at once for a dolphin.
    const auto refill = [&mob, maxAir, path] {
        mob.airSupply = path == "dolphin" ? maxAir : std::min(mob.airSupply + 4, maxAir);
    };
    const auto has = [&mob](std::string_view effect) {
        return mob.effects.contains(wire::common::Identifier::vanilla(std::string{effect}));
    };
    // parity: `MobEffectUtil.hasWaterBreathing` and `shouldEffectsRefillAirsupply`.
    const bool waterBreathing = has("water_breathing") || has("conduit_power") || has("breath_of_the_nautilus");
    const bool effectsRefill = !has("breath_of_the_nautilus") || has("water_breathing") || has("conduit_power");
    if (at.eyeInWater && !at.eyeInBubbles) {
        if (!breathesUnderwater(path) && !waterBreathing) {
            // parity: `decreaseAirSupply`, which only rolls when there is a bonus to roll for.
            const bool spared =
                at.oxygenBonus > 0.0 && at.random != nullptr && at.random->nextDouble() >= 1.0 / (at.oxygenBonus + 1.0);
            if (!spared) {
                mob.airSupply--;
            }
            if (mob.airSupply <= kDrowningAir) {
                mob.airSupply = 0;
                hurts.push_back({kDrownDamage, "drown", true});
            }
        } else if (mob.airSupply < maxAir && effectsRefill) {
            refill();
        }
    } else if (mob.airSupply < maxAir) {
        refill();
    }
    // parity: the `handleAirSupply` overrides.
    const Gasp gasp = gaspOf(path);
    const bool alive = !mob.dying && mob.health > 0.0;
    if (gasp == Gasp::None || (gasp != Gasp::WaterAnimal && mob.noAi)) {
        if (path == "dolphin" && mob.noAi) {
            mob.airSupply = maxAir;
        }
        return hurts;
    }
    const bool wet = gasp == Gasp::Axolotl ? (at.inWater || at.inRain) : at.inWater;
    if (alive && !wet) {
        mob.airSupply = before - 1;
        if (mob.airSupply <= kDrowningAir) {
            mob.airSupply = 0;
            hurts.push_back({kDrownDamage, gasp == Gasp::WaterAnimal ? "drown"sv : "dry_out"sv});
        }
    } else {
        mob.airSupply = gasp == Gasp::Axolotl ? maxAir : ServerEntity::kDefaultMaxAir;
    }
    return hurts;
}

} // namespace basalt::server::runtime::entity::mobBreathing
