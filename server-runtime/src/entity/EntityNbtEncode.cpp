#include "EntityNbtShared.hpp"

#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/Allays.hpp>
#include <basalt/server/runtime/entity/Armadillos.hpp>
#include <basalt/server/runtime/entity/Bees.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/Camels.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/FireworkRockets.hpp>
#include <basalt/server/runtime/entity/Frogs.hpp>
#include <basalt/server/runtime/entity/Goats.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/Minecarts.hpp>
#include <basalt/server/runtime/entity/MobMisc.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/MobStatusEffects.hpp>
#include <basalt/server/runtime/entity/NeutralAnger.hpp>
#include <basalt/server/runtime/entity/Piglins.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Sniffers.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/entity/WitherBoss.hpp>
#include <basalt/server/runtime/entity/villager/VillagerNbt.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <expected>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::entity::entityNbt {

using namespace detail;
using wire::common::Uuid;

NbtCompound encode(const ServerEntity& entity) {
    NbtCompound tag = entity.sourceNbt.has_value() ? *entity.sourceNbt : NbtCompound{};
    tag.remove("Passengers");
    tag.putString("id", entity.type().toString());
    tag.set("UUID", uuidTag(entity.uuid()));
    // parity: `LivingEntity.addAdditionalSaveData` writes `DeathTime` as a short.
    if (entity.deathTime > 0) {
        tag.putShort("DeathTime", static_cast<std::int16_t>(entity.deathTime));
    }
    // parity: `Entity.addAdditionalSaveData` writes `Fire` as a short.
    if (entity.fireTicks > 0) {
        tag.putShort("Fire", static_cast<std::int16_t>(entity.fireTicks));
    }
    // parity: `Entity.addAdditionalSaveData` writes `NoGravity` whenever it is set.
    if (entity.noGravity) {
        tag.putBoolean("NoGravity", true);
    }
    // parity: `Fireball.addAdditionalSaveData` stores the source stack under `Item`.
    const bool arrow = isArrow(entity.type());
    if (entity.heldItem.has_value()) {
        wire::nbt::NbtCompound stack;
        stack.putInt("count", 1);
        stack.putString("id", Identifier::vanilla(*entity.heldItem).toString());
        if (!entity.heldItemComponents.empty()) {
            stack.set("components", wire::nbt::NbtTag::ofCompound(entity.heldItemComponents));
        }
        tag.set(arrow ? "item" : "Item", wire::nbt::NbtTag::ofCompound(std::move(stack)));
    }
    // parity: `AbstractArrow.addAdditionalSaveData`.
    if (arrow) {
        tag.putByte("pickup", static_cast<std::int8_t>(entity.arrowPickup));
        tag.putByte("PierceLevel", entity.pierceLevel);
        tag.putShort("life", static_cast<std::int16_t>(entity.arrowLife));
        tag.putBoolean("inGround", entity.onGround);
        // parity: `ThrownTrident.addAdditionalSaveData`.
        if (entity.type().path() == "trident") {
            tag.putBoolean("DealtDamage", entity.tridentDealtDamage);
        }
    }
    // parity: `Ravager.addAdditionalSaveData`.
    if (entity.type().path() == "ravager") {
        tag.putInt("AttackTick", entity.ravagerAttackTicks);
        tag.putInt("StunTick", entity.ravagerStunTicks);
        tag.putInt("RoarTick", entity.ravagerRoarTicks);
    }
    tag.set("Pos", doubles(entity.x, entity.y, entity.z));
    tag.set("Motion", doubles(entity.velocityX, entity.velocityY, entity.velocityZ));
    tag.set("Rotation", floats(entity.yaw, entity.pitch));
    // parity: Air is `Entity.saveWithoutId`.
    tag.putShort("Air", static_cast<std::int16_t>(entity.airSupply));
    const std::string_view path = entity.type().path();
    if (server::registry::entityAttributes::isLiving(entity.type())) {
        tag.putFloat("Health", static_cast<float>(entity.health));
        if (entity.lastHurtByPlayer.has_value()) {
            tag.set("last_hurt_by_player", uuidTag(*entity.lastHurtByPlayer));
            tag.putInt("last_hurt_by_player_memory_time", entity.lastHurtByPlayerTime);
        }
    }
    if (mobs::isMob(entity.type())) {
        tag.putBoolean("PersistenceRequired", entity.persistent);
        tag.putBoolean("NoAI", entity.noAi);
        tag.putBoolean("CanPickUpLoot", entity.canPickUpLoot);
    }
    if (ageableMobs::isAgeable(path)) {
        tag.putInt("Age", entity.age);
    }
    // parity: `Animal.addAdditionalSaveData`; the wait after breeding is the positive `Age`.
    if (!breeding::foodFor(entity.type()).empty()) {
        tag.putInt("InLove", entity.loveTicks);
    }
    if (taming::isTamable(entity.type())) {
        tag.putBoolean("Sitting", entity.sitting);
    }
    if (path == "tnt") {
        // parity: `PrimedTnt.TAG_FUSE` is lowercase; `Fuse` is a creeper's own swell time.
        tag.putShort("fuse", static_cast<std::int16_t>(entity.fuse));
    }
    // parity: `SulfurCube.addAdditionalSaveData`.
    if (path == "sulfur_cube") {
        tag.putInt("fuse", entity.fuse);
    }
    // parity: `MinecartTNT.addAdditionalSaveData`.
    if (path == "tnt_minecart") {
        tag.putInt("fuse", entity.fuse);
    }
    if (minecarts::isMinecart(entity.type())) {
        minecarts::save(entity, tag);
    }
    // parity: `MinecartHopper.addAdditionalSaveData` always writes `Enabled`.
    if (path == "hopper_minecart" && !tag.contains("Enabled")) {
        tag.putBoolean("Enabled", true);
    }
    if (cubeMobs::isCube(entity.type())) {
        tag.putInt("Size", std::max(entity.cubeSize, 1) - 1);
    }
    // parity: `Zombie`, `Skeleton`, `AbstractPiglin` and `Hoglin`'s conversion clocks.
    if (path == "zombie" || path == "husk") {
        tag.putInt("InWaterTime", entity.conversionClock);
        tag.putInt("DrownedConversionTime", entity.conversionCountdown);
    }
    if (path == "skeleton") {
        tag.putInt("StrayConversionTime", entity.conversionCountdown);
    }
    if (path == "zombie_villager") {
        tag.putInt("ConversionTime", entity.conversionCountdown);
    }
    if (path == "piglin" || path == "piglin_brute" || path == "hoglin") {
        tag.putBoolean("IsImmuneToZombification", entity.immuneToZombification);
        tag.putInt("TimeInOverworld", entity.conversionClock);
    }
    // parity: `Tadpole.addAdditionalSaveData` writes its own `Age`.
    if (path == "tadpole") {
        tag.putInt("Age", entity.conversionClock);
    }
    if (server::registry::entityAttributes::isLiving(entity.type())) {
        if (entity.effects.empty()) {
            tag.remove("active_effects");
        } else {
            tag.set("active_effects", mobStatusEffects::encode(entity));
        }
    }
    // parity: `Creeper.addAdditionalSaveData` writes `powered` whether it is or not.
    if (path == "creeper") {
        tag.putBoolean("powered", entity.powered);
        if (entity.misc != nullptr && entity.misc->creeperIgnited) {
            tag.putBoolean("ignited", true);
        } else {
            tag.remove("ignited");
        }
    }
    if (entity.misc != nullptr && !entity.misc->cubeWanders) {
        tag.putBoolean("Paper.canWander", false);
    }
    if (path == "dolphin") {
        tag.putBoolean("GotFish", entity.gotFish);
        tag.putInt("Moistness", entity.moistness);
    }
    // parity: `Pufferfish.addAdditionalSaveData`, written whether it is puffed or not.
    if (path == "pufferfish") {
        tag.putInt("PuffState", entity.puffState);
    }
    if (entity.wither != nullptr) {
        tag.putInt(std::string{witherBoss::kInvulnerableKey}, entity.wither->invulnerableTicks);
    }
    if (path == "wither_skull") {
        tag.putBoolean("dangerous", entity.dangerousSkull);
    }
    if (path == "firework_rocket") {
        fireworkRockets::write(entity, tag);
    }
    if (path == "bee") {
        bees::write(entity, tag);
    }
    if (path == "allay") {
        allays::write(entity, tag);
    }
    if (piglins::isPiglin(entity.type())) {
        piglins::write(entity, tag);
    }
    neutralAnger::write(entity, tag);
    if (path == "goat") {
        goats::write(entity, tag);
    }
    if (path == "frog") {
        frogs::write(entity, tag);
    }
    if (camels::isCamel(entity.type())) {
        camels::write(entity, tag);
    }
    if (path == "sniffer") {
        sniffers::write(entity, tag);
    }
    if (path == "armadillo") {
        armadillos::write(entity, tag);
    }
    if (patrols(path)) {
        if (entity.patrolTarget.has_value()) {
            tag.putIntArray("patrol_target", {entity.patrolTarget->begin(), entity.patrolTarget->end()});
        } else {
            tag.remove("patrol_target");
        }
        tag.putBoolean("PatrolLeader", entity.patrolLeader);
        tag.putBoolean("Patrolling", entity.patrolling);
        // parity: `Raider.addAdditionalSaveData`.
        tag.putInt("Wave", entity.raidWave);
        tag.putBoolean("CanJoinRaid", entity.canJoinRaid);
        if (entity.raidId.has_value() && !entity.raidStale) {
            tag.putInt("RaidId", *entity.raidId);
        } else {
            tag.remove("RaidId");
        }
    }
    if (path == "wandering_trader" || path == "trader_llama") {
        tag.putInt("DespawnDelay", entity.despawnDelay);
    }
    if (path == "wandering_trader") {
        if (entity.wanderTarget.has_value()) {
            tag.putIntArray("wander_target", {entity.wanderTarget->begin(), entity.wanderTarget->end()});
        } else {
            tag.remove("wander_target");
        }
    }
    if (path == "wandering_trader") {
        const mobMisc::State* held = mobMisc::peek(entity);
        for (const auto& [key, disabled] :
             {std::pair<std::string_view, bool>{"Paper.CanDrinkPotionDisabled",
                                                held != nullptr && held->traderNoPotion},
              std::pair<std::string_view, bool>{"Paper.CanDrinkMilkDisabled", held != nullptr && held->traderNoMilk}}) {
            if (disabled) {
                tag.putBoolean(key, true);
            } else {
                tag.remove(key);
            }
        }
    }
    // parity: `Turtle.addAdditionalSaveData`.
    if (path == "turtle") {
        const mobMisc::State* held = mobMisc::peek(entity);
        if (held != nullptr && held->turtleHome.has_value()) {
            tag.putIntArray("home_pos", {held->turtleHome->begin(), held->turtleHome->end()});
        } else {
            tag.remove("home_pos");
        }
        tag.putBoolean("has_egg", held != nullptr && held->turtleHasEgg);
    }
    if (path == "skeleton_horse") {
        tag.putBoolean("SkeletonTrap", entity.skeletonTrap);
        tag.putInt("SkeletonTrapTime", entity.skeletonTrapTime);
    }
    if (horses::isHorse(entity.type())) {
        tag.putBoolean("Tame", entity.tamedHorse);
    }
    // parity: `LeashData.CODEC`.
    tag.remove("leash");
    if (entity.leashKnotAt.has_value()) {
        tag.putIntArray("leash", {(*entity.leashKnotAt)[0], (*entity.leashKnotAt)[1], (*entity.leashKnotAt)[2]});
    } else if (entity.leashHolderUuid.has_value()) {
        NbtCompound leash;
        // parity: `Leashable.LeashData.CODEC` uses the same four ints for who holds the rope.
        if (const std::optional<wire::common::Uuid> held = wire::common::Uuid::parse(*entity.leashHolderUuid);
            held.has_value()) {
            leash.set("UUID", uuidTag(*held));
        }
        tag.set("leash", NbtTag::ofCompound(std::move(leash)));
    }
    if (path == "villager" || path == "zombie_villager") {
        NbtCompound villagerData = compoundOf(tag, "VillagerData");
        if (villagerData.getString("type").empty()) {
            villagerData.putString("type", "minecraft:plains");
        }
        villagerData.putString("profession", "minecraft:" + entity.profession);
        villagerData.putInt("level", entity.villagerLevel);
        tag.set("VillagerData", NbtTag::ofCompound(std::move(villagerData)));
        tag.putInt("Xp", entity.villagerExperience);
        villagers::writeOffers(entity, tag);
    }
    if (path == "villager") {
        villager::villagerNbt::write(entity, tag);
    }
    if (entity.owner.has_value()) {
        tag.putString("Owner", *entity.owner);
    } else if (entity.ownerUuid.has_value()) {
        // parity: `Projectile.addAdditionalSaveData`'s `Owner` and `LeftOwner`.
        tag.set("Owner", uuidTag(*entity.ownerUuid));
    }
    if (entity.leftOwner) {
        tag.putBoolean("LeftOwner", true);
    }
    if (entity.customName.has_value()) {
        tag.putString("CustomName", *entity.customName);
    }
    if (!entity.tags.empty()) {
        NbtList tags{NbtType::String};
        for (const std::string& name : entity.tags) {
            tags.push(NbtTag::ofString(name));
        }
        tag.set("Tags", NbtTag::ofList(std::move(tags)));
    }
    if (entity.deathLootTable.has_value()) {
        tag.putString("DeathLootTable", *entity.deathLootTable);
    }
    if (entity.deathLootTableSeed != 0) {
        tag.putLong("DeathLootTableSeed", entity.deathLootTableSeed);
    }
    if (!entity.pluginData.empty()) {
        tag.set("PublicBukkitValues", NbtTag::ofCompound(entity.pluginData));
    }
    return tag;
}

} // namespace basalt::server::runtime::entity::entityNbt
