#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <algorithm>
#include <array>

namespace basalt::server::runtime::entity::mobBreathing {
namespace {

using namespace std::string_view_literals;

/// parity: `shouldTakeDrowningDamage`, twenty ticks past empty.
constexpr std::int32_t kDrowningAir = -20;
constexpr float kDrownDamage = 2.0F;
constexpr std::int32_t kAxolotlMaxAir = 6000;
constexpr double kEyeShare = 0.85;
constexpr double kDolphinEye = 0.3;

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
    return mob.type().path() == "dolphin" ? kDolphinEye : mob.height() * kEyeShare;
}

std::vector<Hurt> tick(ServerEntity& mob, const Surroundings& at) {
    const std::string_view path = mob.type().path();
    const std::int32_t maxAir = maxAirOf(path);
    const std::int32_t before = mob.airSupply;
    std::vector<Hurt> hurts;
    // parity: `increaseAirSupply`, four a tick, and all of it at once for a dolphin.
    const auto refill = [&mob, maxAir, path] {
        mob.airSupply = path == "dolphin" ? maxAir : std::min(mob.airSupply + 4, maxAir);
    };
    if (at.eyeInWater && !at.eyeInBubbles) {
        if (!breathesUnderwater(path)) {
            mob.airSupply--;
            if (mob.airSupply <= kDrowningAir) {
                mob.airSupply = 0;
                hurts.push_back({kDrownDamage, "drown"});
            }
        } else if (mob.airSupply < maxAir) {
            refill();
        }
    } else if (mob.airSupply < maxAir) {
        refill();
    }
    // parity: the `handleAirSupply` overrides, which start again from the air before the tick.
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
