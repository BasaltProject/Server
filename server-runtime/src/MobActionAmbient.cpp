#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Armadillos.hpp>
#include <basalt/server/runtime/entity/Axolotls.hpp>
#include <basalt/server/runtime/entity/Creakings.hpp>
#include <basalt/server/runtime/entity/Hoglins.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/MobSounds.hpp>
#include <basalt/server/runtime/entity/NeutralAnger.hpp>
#include <basalt/server/runtime/entity/Piglins.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Shulkers.hpp>
#include <basalt/server/runtime/entity/Sniffers.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <array>
#include <cmath>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
namespace mth = wire::common::mth;
using namespace std::string_view_literals;

/// parity: `Parrot.MOB_SOUND_MAP`'s sounds.
/// hash map whose order depends on object identities, so no order is the jar's. The happy ghast's
/// is the empty one.
constexpr std::array kImitations{"blaze"sv,
                                 "bogged"sv,
                                 "breeze"sv,
                                 "camel_husk"sv,
                                 "spider"sv,
                                 "creaking"sv,
                                 "creeper"sv,
                                 "drowned"sv,
                                 "elder_guardian"sv,
                                 "ender_dragon"sv,
                                 "endermite"sv,
                                 "evoker"sv,
                                 "ghast"sv,
                                 "guardian"sv,
                                 ""sv,
                                 "hoglin"sv,
                                 "husk"sv,
                                 "illusioner"sv,
                                 "magma_cube"sv,
                                 "parched"sv,
                                 "phantom"sv,
                                 "piglin"sv,
                                 "piglin_brute"sv,
                                 "pillager"sv,
                                 "ravager"sv,
                                 "shulker"sv,
                                 "silverfish"sv,
                                 "skeleton"sv,
                                 "slime"sv,
                                 "spider"sv,
                                 "stray"sv,
                                 "vex"sv,
                                 "vindicator"sv,
                                 "warden"sv,
                                 "witch"sv,
                                 "wither"sv,
                                 "wither_skeleton"sv,
                                 "zoglin"sv,
                                 "zombie"sv,
                                 "zombie_horse"sv,
                                 "zombie_nautilus"sv,
                                 "zombie_villager"sv};

[[nodiscard]] bool isBaby(const ServerEntity& mob) { return mob.age < 0; }

[[nodiscard]] bool eyesInWater(const world::WorldState& world, const ServerEntity& mob) {
    const double eyes = mob.y + entity::mobBreathing::eyeHeightOf(mob);
    const std::optional<std::int32_t> state = world.blockState(mth::floor(mob.x), mth::floor(eyes), mth::floor(mob.z));
    return state.has_value() && world::fluids::of(*state) == world::Submersion::Water;
}

/// parity: `Panda.Gene.getVariantFromGenes`.
[[nodiscard]] std::string_view pandaVariant(const ServerEntity& panda) {
    if (!panda.sourceNbt.has_value()) {
        return "normal";
    }
    const std::string_view main = panda.sourceNbt->getString("MainGene");
    const std::string_view hidden = panda.sourceNbt->getString("HiddenGene");
    if (main == "brown" || main == "weak") {
        return main == hidden ? main : "normal";
    }
    return main.empty() ? "normal"sv : main;
}

} // namespace

std::optional<std::string> MobActionSystem::ambientSoundOf(world::LoadedWorld& world, ServerEntity& mob,
                                                           random::RandomSource& random) {
    const std::string path{mob.type().path()};
    const std::int64_t now = world.gameTime();
    const bool inWater = entity::ai::inWater(world.state(), mob);
    const auto own = [&path](std::string_view name) { return "entity." + path + "." + std::string{name}; };
    if (path == "cow" || path == "mooshroom" || path == "pig" || path == "chicken") {
        return entity::mobSounds::variantSound(mob, "ambient");
    }
    if (path == "cat") {
        // parity: `Cat.getAmbientSound`.
        if (!mob.owner.has_value()) {
            return entity::mobSounds::variantSound(mob, "stray_ambient");
        }
        if (mob.loveTicks > 0) {
            return entity::mobSounds::variantSound(mob, "purr");
        }
        return entity::mobSounds::variantSound(mob, random.nextInt(4) == 0 ? "purreow" : "ambient");
    }
    if (path == "wolf") {
        // parity: `Wolf.getAmbientSound`.
        const bool angry = mob.neutral != nullptr && entity::neutralAnger::isAngry(*mob.neutral, now);
        if (angry) {
            return entity::mobSounds::variantSound(mob, "growl");
        }
        if (random.nextInt(3) == 0) {
            return entity::mobSounds::variantSound(mob, mob.owner.has_value() && mob.health < 20.0 ? "whine" : "pant");
        }
        return entity::mobSounds::variantSound(mob, "ambient");
    }
    if (path == "fox") {
        // parity: `Fox.getAmbientSound`.
        if (mob.sleeping) {
            return own("sleep");
        }
        if (world.skyDarken() >= 4 && random.nextFloat() < 0.1F) {
            bool alone = true;
            for (const player::PlayerSession* player : watchersOf(world)) {
                if (player->gamemode != wire::protocol::play::Gamemode::Spectator &&
                    std::abs(player->x - mob.x) < 16.0 + (mob.width() / 2.0) + 0.3 &&
                    std::abs(player->z - mob.z) < 16.0 + (mob.width() / 2.0) + 0.3 && player->y + 1.8 > mob.y - 16.0 &&
                    player->y < mob.y + mob.height() + 16.0) {
                    alone = false;
                }
            }
            if (alone) {
                return own("screech");
            }
        }
        return own("ambient");
    }
    if (path == "parrot") {
        // parity: `Parrot.getAmbient`.
        if (world.difficulty() != "peaceful" && random.nextInt(1000) == 0) {
            const std::string_view imitated =
                kImitations[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kImitations.size())))];
            return imitated.empty() ? std::string{"intentionally_empty"}
                                    : "entity.parrot.imitate." + std::string{imitated};
        }
        return own("ambient");
    }
    if (path == "villager" || path == "wandering_trader") {
        if (mob.sleeping) {
            return std::nullopt;
        }
        const bool trading = mob.villager != nullptr && mob.villager->tradingPlayer.has_value();
        return own(trading ? "trade" : "ambient");
    }
    if (path == "warden") {
        // parity: `Warden.getAmbientSound`.
        if (entity::wardenBrain::poseOf(mob) == wire::protocol::play::Pose::Roaring ||
            entity::wardenBrain::isDiggingOrEmerging(mob)) {
            return std::nullopt;
        }
        std::int32_t anger = 0;
        if (mob.targetId.has_value()) {
            anger = entity::wardens::angerAt(mob, *mob.targetId);
        } else if (mob.anger.has_value() && mob.anger->size() == 1) {
            anger = mob.anger->begin()->second;
        }
        return own(anger >= 80 ? "angry" : anger >= 40 ? "agitated" : "ambient");
    }
    if (path == "piglin" || path == "piglin_brute") {
        // parity: `AbstractPiglin.playAmbientSound`.
        if (mob.piglin != nullptr && mob.piglin->activity != entity::piglins::Activity::Idle) {
            return std::nullopt;
        }
        return own(path == "piglin" && mob.conversionClock > 0 ? "retreat" : "ambient");
    }
    if (path == "hoglin") {
        // parity: `HoglinAi.getSoundForCurrentActivity`.
        const entity::hoglins::Activity activity =
            mob.hoglin == nullptr ? entity::hoglins::Activity::Idle : mob.hoglin->activity;
        if (activity == entity::hoglins::Activity::Avoid || mob.conversionClock > 0) {
            return own("retreat");
        }
        return own(activity == entity::hoglins::Activity::Fight ? "angry" : "ambient");
    }
    if (path == "zoglin") {
        return own(mob.targetId.has_value() ? "angry" : "ambient");
    }
    if (path == "zombified_piglin") {
        return own(mob.neutral != nullptr && entity::neutralAnger::isAngry(*mob.neutral, now) ? "angry" : "ambient");
    }
    if (path == "guardian" || path == "elder_guardian" || path == "dolphin" || path == "drowned") {
        if (inWater) {
            return own(path == "dolphin" || path == "drowned" ? "ambient_water" : "ambient");
        }
        return own(path == "dolphin" || path == "drowned" ? "ambient" : "ambient_land");
    }
    if (path == "axolotl") {
        if (entity::axolotls::playingDead(mob)) {
            return std::nullopt;
        }
        return own(inWater ? "idle_water" : "idle_air");
    }
    if (path == "skeleton_horse") {
        return own(eyesInWater(world.state(), mob) ? "ambient_water" : "ambient");
    }
    if (path == "nautilus" || path == "zombie_nautilus") {
        const bool under = eyesInWater(world.state(), mob);
        const std::string base = path == "nautilus" && isBaby(mob) ? "entity.baby_nautilus." : "entity." + path + ".";
        return base + (under ? "ambient" : "ambient_land");
    }
    if (path == "turtle") {
        return !inWater && mob.onGround && !isBaby(mob) ? std::optional{own("ambient_land")} : std::nullopt;
    }
    if (path == "horse") {
        return isBaby(mob) ? std::string{"entity.baby_horse.ambient"} : own("ambient");
    }
    if (path == "polar_bear") {
        return own(isBaby(mob) ? "ambient_baby" : "ambient");
    }
    if (path == "happy_ghast") {
        return isBaby(mob) ? std::string{"entity.ghastling.ambient"} : own("ambient");
    }
    if (path == "goat") {
        const bool screaming = mob.sourceNbt.has_value() && mob.sourceNbt->getBoolean("IsScreamingGoat");
        return own(screaming ? "screaming.ambient" : "ambient");
    }
    if (path == "allay") {
        const bool holding = entity::mobEquipment::get(mob, entity::mobEquipment::Slot::MainHand).has_value();
        return own(holding ? "ambient_with_item" : "ambient_without_item");
    }
    if (path == "panda") {
        const std::string_view variant = pandaVariant(mob);
        return own(variant == "aggressive" ? "aggressive_ambient"
                   : variant == "worried"  ? "worried_ambient"
                                           : "ambient");
    }
    if (path == "sniffer") {
        const bool busy = mob.sniffer != nullptr && (mob.sniffer->pose == entity::sniffers::Pose::Digging ||
                                                     mob.sniffer->pose == entity::sniffers::Pose::Searching);
        return busy ? std::nullopt : std::optional{own("idle")};
    }
    if (path == "armadillo") {
        return entity::armadillos::isScared(mob) ? std::nullopt : std::optional{own("ambient")};
    }
    if (path == "bat") {
        return mob.flight.resting && random.nextInt(4) != 0 ? std::nullopt : std::optional{own("ambient")};
    }
    if (path == "strider") {
        // parity: `Strider.getAmbientSound`, silent while panicking or while a `TemptGoal` follows somebody.
        constexpr double kTemptRange = 10.0;
        bool tempted = false;
        for (const player::PlayerSession* player : watchersOf(world)) {
            const std::optional<storage::StoredItemStack>& held = player->inventory.heldItem();
            const double dx = player->x - mob.x;
            const double dy = player->y - mob.y;
            const double dz = player->z - mob.z;
            tempted =
                tempted || (held.has_value() && player->gamemode != wire::protocol::play::Gamemode::Spectator &&
                            (held->id == "minecraft:warped_fungus" || held->id == "minecraft:warped_fungus_on_stick") &&
                            (dx * dx) + (dy * dy) + (dz * dz) <= kTemptRange * kTemptRange);
        }
        return mob.panicTicks > 0 || tempted ? std::nullopt : std::optional{own("ambient")};
    }
    if (path == "creaking") {
        // parity: `Creaking.getAmbientSound`, silent while it is active.
        return mob.creaking != nullptr && mob.creaking->active ? std::nullopt : std::optional{own("ambient")};
    }
    if (path == "shulker") {
        // parity: `Shulker.playAmbientSound`, silent while shut.
        return mob.shulker != nullptr && mob.shulker->peek == 0 ? std::nullopt : std::optional{own("ambient")};
    }
    if (path == "cave_spider") {
        return std::string{"entity.spider.ambient"};
    }
    if (path == "trader_llama") {
        return std::string{"entity.llama.ambient"};
    }
    // parity: `Enderman.getAmbientSound`, the scream while `isCreepy`.
    if (path == "enderman" && mob.targetId.has_value()) {
        return own("scream");
    }
    if (path == "enderman" || path == "endermite" || path == "silverfish" || path == "blaze" || path == "ravager" ||
        path == "illusioner" || path == "stray" || path == "parched" || path == "zombie_villager" || path == "vex" ||
        path == "pillager" || path == "skeleton" || path == "frog" || path == "phantom" || path == "vindicator" ||
        path == "wither_skeleton" || path == "zombie_horse" || path == "mule" || path == "bogged" || path == "evoker" ||
        path == "husk" || path == "llama" || path == "donkey" || path == "squid" || path == "zombie" ||
        path == "snow_golem" || path == "camel" || path == "camel_husk" || path == "tropical_fish" ||
        path == "ender_dragon" || path == "ocelot" || path == "rabbit" || path == "sheep" || path == "salmon" ||
        path == "glow_squid" || path == "cod" || path == "wither" || path == "witch" || path == "spider" ||
        path == "ghast") {
        return own("ambient");
    }
    return std::nullopt;
}

void MobActionSystem::tickAmbientSound(world::LoadedWorld& world, ServerEntity& mob, random::RandomSource& random) {
    // parity: `Mob.baseTick`.
    if (mob.dying || mob.health <= 0.0 || random.nextInt(1000) >= mob.ambientSoundTime++) {
        return;
    }
    mob.ambientSoundTime = -entity::mobSounds::ambientInterval(mob);
    const std::optional<std::string> sound = ambientSoundOf(world, mob, random);
    if (!sound.has_value() || sound->empty() || (mob.sourceNbt.has_value() && mob.sourceNbt->getBoolean("Silent"))) {
        return;
    }
    // parity: `Fox.playAmbientSound`, whose screech is twice as loud.
    const float volume = *sound == "entity.fox.screech" ? 2.0F : entity::mobSounds::volume(mob);
    const float pitch = entity::mobSounds::voicePitch(mob, random);
    for (player::PlayerSession* player : watchersOf(world)) {
        if (player->connection().playSound) {
            player->connection().playSound(*sound, mob.x, mob.y, mob.z, volume, pitch);
            continue;
        }
        if (player->connection().sendPlayPacket) {
            const wire::protocol::play::SoundPacket packet{.sound = *sound,
                                                           .source = entity::mobSounds::source(mob),
                                                           .x = mob.x,
                                                           .y = mob.y,
                                                           .z = mob.z,
                                                           .volume = volume,
                                                           .pitch = pitch,
                                                           .seed = random.nextLong()};
            player->connection().sendPlayPacket(wire::protocol::play::SoundPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

} // namespace basalt::server::runtime
