#include <basalt/server/runtime/world/PotionEffects.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <string>

namespace basalt::server::runtime::world::potionEffects {
namespace {

using wire::common::Identifier;

struct Entry {
    std::string_view potion;
    std::string_view effect;
    std::int32_t duration;
    std::int32_t amplifier;
};

/// parity: `Potions`, what each registered potion holds. Turtle master is the one with two.
constexpr std::array<Entry, 45> kPotions{{
    {"night_vision", "night_vision", 3600, 0},
    {"long_night_vision", "night_vision", 9600, 0},
    {"invisibility", "invisibility", 3600, 0},
    {"long_invisibility", "invisibility", 9600, 0},
    {"leaping", "jump_boost", 3600, 0},
    {"long_leaping", "jump_boost", 9600, 0},
    {"strong_leaping", "jump_boost", 1800, 1},
    {"fire_resistance", "fire_resistance", 3600, 0},
    {"long_fire_resistance", "fire_resistance", 9600, 0},
    {"swiftness", "speed", 3600, 0},
    {"long_swiftness", "speed", 9600, 0},
    {"strong_swiftness", "speed", 1800, 1},
    {"slowness", "slowness", 1800, 0},
    {"long_slowness", "slowness", 4800, 0},
    {"strong_slowness", "slowness", 400, 3},
    {"turtle_master", "slowness", 400, 3},
    {"turtle_master", "resistance", 400, 2},
    {"long_turtle_master", "slowness", 800, 3},
    {"long_turtle_master", "resistance", 800, 2},
    {"strong_turtle_master", "slowness", 400, 5},
    {"strong_turtle_master", "resistance", 400, 3},
    {"water_breathing", "water_breathing", 3600, 0},
    {"long_water_breathing", "water_breathing", 9600, 0},
    {"healing", "instant_health", 1, 0},
    {"strong_healing", "instant_health", 1, 1},
    {"harming", "instant_damage", 1, 0},
    {"strong_harming", "instant_damage", 1, 1},
    {"poison", "poison", 900, 0},
    {"long_poison", "poison", 1800, 0},
    {"strong_poison", "poison", 432, 1},
    {"regeneration", "regeneration", 900, 0},
    {"long_regeneration", "regeneration", 1800, 0},
    {"strong_regeneration", "regeneration", 450, 1},
    {"strength", "strength", 3600, 0},
    {"long_strength", "strength", 9600, 0},
    {"strong_strength", "strength", 1800, 1},
    {"weakness", "weakness", 1800, 0},
    {"long_weakness", "weakness", 4800, 0},
    {"luck", "luck", 6000, 0},
    {"slow_falling", "slow_falling", 1800, 0},
    {"long_slow_falling", "slow_falling", 4800, 0},
    {"wind_charged", "wind_charged", 3600, 0},
    {"weaving", "weaving", 3600, 0},
    {"oozing", "oozing", 3600, 0},
    {"infested", "infested", 3600, 0},
}};

/// parity: `MobEffectInstance.withScaledDuration`; an infinite one stays infinite.
[[nodiscard]] ActiveEffect scaled(ActiveEffect effect, float scale) {
    if (effect.remainingTicks >= 0) {
        effect.remainingTicks = std::max(
            wire::common::mth::floor(static_cast<double>(static_cast<float>(effect.remainingTicks) * scale)), 1);
    }
    return effect;
}

} // namespace

std::vector<Identifier> ids() {
    // parity: the three that carry nothing come first, then every potion in `kPotions`, which is
    // in registration order with turtle master's two effects side by side.
    std::vector<Identifier> all{Identifier::vanilla("water"), Identifier::vanilla("mundane"),
                                Identifier::vanilla("thick"), Identifier::vanilla("awkward")};
    for (const Entry& entry : kPotions) {
        const Identifier id = Identifier::vanilla(std::string{entry.potion});
        if (all.back() != id) {
            all.push_back(id);
        }
    }
    return all;
}

std::vector<ActiveEffect> ofPotion(std::string_view potion) {
    std::vector<ActiveEffect> effects;
    const auto add = [&effects](const Entry& entry) {
        effects.push_back(ActiveEffect{.effect = Identifier::vanilla(std::string{entry.effect}),
                                       .amplifier = entry.amplifier,
                                       .remainingTicks = entry.duration});
    };
    for (const Entry& entry : kPotions) {
        if (entry.potion == potion) {
            add(entry);
        }
    }
    return effects;
}

std::vector<ActiveEffect> of(const wire::nbt::NbtCompound& potionContents, float durationScale) {
    std::vector<ActiveEffect> effects;
    if (const std::string_view potion = potionContents.getString("potion"); !potion.empty()) {
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(potion);
        if (id.has_value()) {
            for (const ActiveEffect& effect : ofPotion(id->path())) {
                effects.push_back(scaled(effect, durationScale));
            }
        }
    }
    const wire::nbt::NbtList* custom = potionContents.getList("custom_effects");
    if (custom == nullptr) {
        return effects;
    }
    for (const wire::nbt::NbtTag& entry : *custom) {
        const wire::nbt::NbtCompound* effect = entry.get<wire::nbt::NbtCompound>();
        if (effect == nullptr) {
            continue;
        }
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(effect->getString("id"));
        if (!id.has_value()) {
            continue;
        }
        const bool particles = effect->getByte("show_particles", 1) != 0;
        effects.push_back(scaled(ActiveEffect{.effect = *id,
                                              .amplifier = effect->getByte("amplifier"),
                                              .remainingTicks = effect->getInt("duration", 1),
                                              .ambient = effect->getByte("ambient") != 0,
                                              .visible = particles,
                                              .showIcon = effect->getByte("show_icon", particles ? 1 : 0) != 0},
                                 durationScale));
    }
    return effects;
}

} // namespace basalt::server::runtime::world::potionEffects
