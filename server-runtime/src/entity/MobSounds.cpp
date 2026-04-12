#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/MobSounds.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <array>
#include <string_view>

namespace basalt::server::runtime::entity::mobSounds {
namespace {

using namespace std::string_view_literals;

constexpr std::array kHorses{"horse"sv, "donkey"sv,     "mule"sv,           "llama"sv,       "trader_llama"sv,
                             "camel"sv, "camel_husk"sv, "skeleton_horse"sv, "zombie_horse"sv};

/// parity: the `Animal`, `WaterAnimal`, `AgeableWaterCreature` and `AbstractGolem` subclasses, all
/// at a hundred and twenty.
constexpr std::array kSlowTalkers{
    "armadillo"sv, "axolotl"sv,       "bee"sv,         "cat"sv,          "chicken"sv,         "cow"sv,
    "fox"sv,       "frog"sv,          "goat"sv,        "hoglin"sv,       "mooshroom"sv,       "panda"sv,
    "parrot"sv,    "pig"sv,           "polar_bear"sv,  "rabbit"sv,       "sheep"sv,           "sniffer"sv,
    "strider"sv,   "wolf"sv,          "happy_ghast"sv, "nautilus"sv,     "zombie_nautilus"sv, "cod"sv,
    "salmon"sv,    "tropical_fish"sv, "pufferfish"sv,  "tadpole"sv,      "squid"sv,           "glow_squid"sv,
    "dolphin"sv,   "iron_golem"sv,    "snow_golem"sv,  "copper_golem"sv, "shulker"sv};

constexpr std::array kQuiet{"allay"sv, "wolf"sv, "bee"sv, "squid"sv, "glow_squid"sv, "cow"sv, "mooshroom"sv};

template <std::size_t N>
[[nodiscard]] bool among(const std::array<std::string_view, N>& list, std::string_view path) {
    for (const std::string_view entry : list) {
        if (entry == path) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool isBaby(const ServerEntity& mob) { return mob.age < 0; }

} // namespace

std::int32_t ambientInterval(const ServerEntity& mob) {
    const std::string_view path = mob.type().path();
    if (among(kHorses, path)) {
        return 400;
    }
    if (path == "ocelot") {
        return 900;
    }
    if (path == "turtle") {
        return 200;
    }
    if (path == "guardian" || path == "elder_guardian") {
        return 160;
    }
    // parity: `HappyGhast.getAmbientSoundInterval`, six times as long with somebody aboard.
    if (path == "happy_ghast") {
        return mob.passengerIds.empty() ? 120 : 720;
    }
    return among(kSlowTalkers, path) ? 120 : 80;
}

float volume(const ServerEntity& mob) {
    const std::string_view path = mob.type().path();
    if (path == "ghast" || path == "ender_dragon") {
        return 5.0F;
    }
    if (path == "warden") {
        return 4.0F;
    }
    if (path == "happy_ghast") {
        return isBaby(mob) ? 1.0F : 4.0F;
    }
    if (path == "slime" || path == "magma_cube" || path == "sulfur_cube") {
        return 0.4F * static_cast<float>(mob.cubeSize);
    }
    if (path == "bat") {
        return 0.1F;
    }
    if (among(kHorses, path)) {
        return 0.8F;
    }
    return among(kQuiet, path) ? 0.4F : 1.0F;
}

float voicePitch(const ServerEntity& mob, random::RandomSource& random) {
    const std::string_view path = mob.type().path();
    if (path == "happy_ghast") {
        return 1.0F;
    }
    const float spread = (random.nextFloat() - random.nextFloat()) * 0.2F;
    if (path == "zombie_villager") {
        return spread + (isBaby(mob) ? 2.0F : 1.0F);
    }
    // parity: `Parrot.getPitch`, which never counts it a baby.
    if (path == "parrot") {
        return spread + 1.0F;
    }
    const float pitch = spread + (isBaby(mob) ? 1.5F : 1.0F);
    return path == "bat" ? pitch * 0.95F : pitch;
}

wire::protocol::play::SoundSource source(const ServerEntity& mob) {
    using wire::protocol::play::SoundSource;
    const std::string_view path = mob.type().path();
    if (path == "sulfur_cube") {
        return SoundSource::Neutral;
    }
    // parity: `Rabbit.getSoundSource`, hostile for the killer bunny.
    if (path == "rabbit") {
        return mob.sourceNbt.has_value() && mob.sourceNbt->getInt("RabbitType") == 99 ? SoundSource::Hostile
                                                                                      : SoundSource::Neutral;
    }
    return mobs::categoryOf(mob.type()) == "monster" ? SoundSource::Hostile : SoundSource::Neutral;
}

std::string variantSound(const ServerEntity& mob, std::string_view kind) {
    const std::string_view path = mob.type().path();
    const bool cow = path == "cow" || path == "mooshroom";
    if (!cow && path != "pig" && path != "chicken" && path != "cat" && path != "wolf") {
        return {};
    }
    const std::string_view base = cow ? "cow"sv : path;
    // parity: the cow's set has no baby half; the others share one baby set across every variant.
    if (!cow && isBaby(mob)) {
        return "entity.baby_" + std::string{base} + "." + std::string{kind};
    }
    std::string variant = mob.sourceNbt.has_value() ? std::string{mob.sourceNbt->getString("sound_variant")} : "";
    if (const std::size_t colon = variant.find(':'); colon != std::string::npos) {
        variant = variant.substr(colon + 1);
    }
    if (variant.empty() || variant == "classic") {
        return "entity." + std::string{base} + "." + std::string{kind};
    }
    return "entity." + std::string{base} + "_" + variant + "." + std::string{kind};
}

} // namespace basalt::server::runtime::entity::mobSounds
