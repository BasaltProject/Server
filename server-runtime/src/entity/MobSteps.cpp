#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/MobSounds.hpp>
#include <basalt/server/runtime/entity/MobSteps.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <algorithm>
#include <array>

namespace basalt::server::runtime::entity::mobSteps {
namespace {

using namespace std::string_view_literals;

/// parity: `Monster.getSwimSound` and `getFallSounds`, whose classes these are.
constexpr std::array kMonsters{
    "blaze"sv,    "bogged"sv,         "breeze"sv,          "cave_spider"sv,     "creaking"sv, "creeper"sv,
    "drowned"sv,  "elder_guardian"sv, "enderman"sv,        "endermite"sv,       "evoker"sv,   "giant"sv,
    "guardian"sv, "husk"sv,           "illusioner"sv,      "parched"sv,         "piglin"sv,   "piglin_brute"sv,
    "pillager"sv, "ravager"sv,        "silverfish"sv,      "skeleton"sv,        "stray"sv,    "spider"sv,
    "vex"sv,      "vindicator"sv,     "warden"sv,          "witch"sv,           "wither"sv,   "wither_skeleton"sv,
    "zoglin"sv,   "zombie"sv,         "zombie_villager"sv, "zombified_piglin"sv};

/// parity: the plain `playSound(STEP, 0.15, 1)` overrides.
constexpr std::array<std::pair<std::string_view, std::string_view>, 26> kOwnSteps{{
    {"endermite", "entity.endermite.step"},
    {"creaking", "entity.creaking.step"},
    {"ravager", "entity.ravager.step"},
    {"skeleton", "entity.skeleton.step"},
    {"stray", "entity.stray.step"},
    {"wither_skeleton", "entity.wither_skeleton.step"},
    {"bogged", "entity.bogged.step"},
    {"parched", "entity.parched.step"},
    {"silverfish", "entity.silverfish.step"},
    {"piglin", "entity.piglin.step"},
    {"hoglin", "entity.hoglin.step"},
    {"spider", "entity.spider.step"},
    {"cave_spider", "entity.spider.step"},
    {"zoglin", "entity.zoglin.step"},
    {"polar_bear", "entity.polar_bear.step"},
    {"piglin_brute", "entity.piglin_brute.step"},
    {"zombie", "entity.zombie.step"},
    {"husk", "entity.husk.step"},
    {"drowned", "entity.drowned.step"},
    {"zombie_villager", "entity.zombie_villager.step"},
    {"llama", "entity.llama.step"},
    {"trader_llama", "entity.llama.step"},
    {"frog", "entity.frog.step"},
    {"parrot", "entity.parrot.step"},
    {"armadillo", "entity.armadillo.step"},
    {"goat", "entity.goat.step"},
}};

constexpr std::array kMoreOwnSteps{std::pair{"panda"sv, "entity.panda.step"sv},
                                   std::pair{"sniffer"sv, "entity.sniffer.step"sv},
                                   std::pair{"sheep"sv, "entity.sheep.step"sv}};

/// parity: the empty `playStepSound` of the mobs that make no footstep at all.
constexpr std::array kSilentSteps{
    "allay"sv,      "bee"sv,           "happy_ghast"sv, "cod"sv,      "salmon"sv,
    "pufferfish"sv, "tropical_fish"sv, "tadpole"sv,     "nautilus"sv, "zombie_nautilus"sv};

[[nodiscard]] bool isMonster(std::string_view path) { return std::ranges::find(kMonsters, path) != kMonsters.end(); }

[[nodiscard]] Sound quiet(std::string name) { return Sound{.name = std::move(name), .volume = 0.15F}; }

/// parity: `AbstractHorse.isWoodSoundType`.
[[nodiscard]] bool woodSounding(const registry::BlockSoundTypeEntry& sound) {
    return sound.stepSound == "block.wood.step" || sound.stepSound == "block.nether_wood.step" ||
           sound.stepSound == "block.stem.step" || sound.stepSound == "block.cherry_wood.step" ||
           sound.stepSound == "block.bamboo_wood.step";
}

[[nodiscard]] std::optional<Sound> horseStep(const ServerEntity& mob, std::int32_t stateId, std::int32_t above) {
    // parity: `AbstractHorse.playStepSound`.
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block != nullptr && (block->path() == "water" || block->path() == "lava")) {
        return Sound{};
    }
    const wire::common::Identifier* over = registry::blockStateDefinitions::blockOf(above);
    const registry::BlockSoundTypeEntry& sound =
        registry::blockSounds::of(over != nullptr && over->path() == "snow" ? above : stateId);
    const float volume = sound.volume * 0.15F;
    if (woodSounding(sound)) {
        return Sound{.name = "entity.horse.step_wood", .volume = volume, .pitch = sound.pitch};
    }
    return Sound{
        .name = mob.age < 0 ? "entity.horse.step_baby" : "entity.horse.step", .volume = volume, .pitch = sound.pitch};
}

} // namespace

Emission emissionOf(const wire::common::Identifier& type) {
    const std::string_view path = type.path();
    if (path == "shulker" || path == "experience_orb" || path == "fishing_bobber" || path == "arrow" ||
        path == "spectral_arrow" || path == "trident") {
        return Emission::None;
    }
    if (path == "silverfish" || path == "endermite" || path == "guardian" || path == "elder_guardian" ||
        path == "breeze" || path.ends_with("boat") || path.ends_with("raft") || path.ends_with("minecart")) {
        return Emission::Events;
    }
    return Emission::All;
}

std::optional<Sound> ownStep(const ServerEntity& mob, std::int32_t stateId, std::int32_t above, bool inLava,
                             const Tagged& tagged) {
    const std::string_view path = mob.type().path();
    if (std::ranges::find(kSilentSteps, path) != kSilentSteps.end()) {
        return Sound{};
    }
    for (const auto& [type, sound] : kOwnSteps) {
        if (type == path) {
            return quiet(std::string{sound});
        }
    }
    for (const auto& [type, sound] : kMoreOwnSteps) {
        if (type == path) {
            return quiet(std::string{sound});
        }
    }
    if (path == "cow" || path == "mooshroom" || path == "pig" || path == "chicken" || path == "wolf") {
        return quiet(mobSounds::variantSound(mob, "step"));
    }
    if (path == "turtle") {
        return quiet(mob.age < 0 ? "entity.turtle.shamble_baby" : "entity.turtle.shamble");
    }
    if (path == "warden") {
        return Sound{.name = "entity.warden.step", .volume = 10.0F};
    }
    if (path == "iron_golem") {
        return Sound{.name = "entity.iron_golem.step"};
    }
    if (path == "strider") {
        return Sound{.name = inLava ? "entity.strider.step_lava" : "entity.strider.step"};
    }
    if (path == "camel" || path == "camel_husk") {
        const bool sand = tagged && tagged("minecraft:camel_sand_step_sound_blocks", stateId);
        const std::string prefix = path == "camel" ? "entity.camel." : "entity.camel_husk.";
        return Sound{.name = prefix + (sand ? "step_sand" : "step"), .volume = path == "camel" ? 1.0F : 0.4F};
    }
    if (mob.copperGolem != nullptr) {
        switch (mob.copperGolem->weather) {
        case copperGolems::Weather::Weathered:
            return Sound{.name = "entity.copper_golem_weathered.step"};
        case copperGolems::Weather::Oxidized:
            return Sound{.name = "entity.copper_golem_oxidized.step"};
        default:
            return Sound{.name = "entity.copper_golem.step"};
        }
    }
    if (path == "horse" || path == "donkey" || path == "mule" || path == "skeleton_horse" || path == "zombie_horse") {
        return horseStep(mob, stateId, above);
    }
    return std::nullopt;
}

Sound blockStep(std::int32_t stateId) {
    const registry::BlockSoundTypeEntry& sound = registry::blockSounds::of(stateId);
    return Sound{.name = std::string{sound.stepSound}, .volume = sound.volume * 0.15F, .pitch = sound.pitch};
}

std::string swimSound(const ServerEntity& mob) {
    const std::string_view path = mob.type().path();
    if (path == "dolphin") {
        return "entity.dolphin.swim";
    }
    if (path == "drowned") {
        return "entity.drowned.swim";
    }
    if (path == "cod" || path == "salmon" || path == "pufferfish" || path == "tropical_fish" || path == "tadpole") {
        return "entity.fish.swim";
    }
    if (path == "turtle") {
        return "entity.turtle.swim";
    }
    if (path == "nautilus") {
        return mob.age < 0 ? "entity.baby_nautilus.swim" : "entity.nautilus.swim";
    }
    if (path == "zombie_nautilus") {
        return "entity.zombie_nautilus.swim";
    }
    if (path == "skeleton_horse") {
        return mob.onGround ? "entity.skeleton_horse.step_water" : "entity.skeleton_horse.swim";
    }
    if (path == "hoglin" || isMonster(path)) {
        return "entity.hostile.swim";
    }
    return "entity.generic.swim";
}

float swimVolume(const ServerEntity& mob, float volume) {
    const std::string_view path = mob.type().path();
    if (path == "turtle") {
        return volume * 1.5F;
    }
    if (path == "skeleton_horse") {
        return mob.onGround ? 0.3F : std::min(0.1F, volume * 25.0F);
    }
    return volume;
}

bool countsFalls(const wire::common::Identifier& type) {
    const std::string_view path = type.path();
    return path != "allay" && path != "parrot" && path != "bee" && path != "bat" && path != "ghast" &&
           path != "happy_ghast" && path != "phantom";
}

std::string fallSound(const wire::common::Identifier& type, std::int32_t damage) {
    const bool big = damage > 4;
    if (isMonster(type.path())) {
        return big ? "entity.hostile.big_fall" : "entity.hostile.small_fall";
    }
    return big ? "entity.generic.big_fall" : "entity.generic.small_fall";
}

std::int32_t fallDamageReduction(const wire::common::Identifier& type) {
    if (type.path() == "goat") {
        return 10;
    }
    return type.path() == "frog" ? 5 : 0;
}

} // namespace basalt::server::runtime::entity::mobSteps
