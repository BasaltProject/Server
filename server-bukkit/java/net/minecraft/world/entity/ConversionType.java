// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.world.item.ItemStack;
public enum ConversionType {
    SINGLE, SPLIT_ON_DEATH;
    private boolean discardsOriginal() {
        return this == SINGLE;
    }
    private static void convertCommon(Mob donor, Mob heir, ConversionParams params) {
        heir.setAbsorptionAmount(donor.getAbsorptionAmount());
        for (var effect : donor.getActiveEffects()) {
            heir.addEffect(new net.minecraft.world.effect.MobEffectInstance(effect));
        }
        if (donor.isBaby()) {
            heir.setBaby(true);
        }
        if (donor instanceof AgeableMob oldAge && heir instanceof AgeableMob newAge) {
            newAge.setAge(oldAge.getAge());
            newAge.forcedAge = oldAge.forcedAge;
            newAge.forcedAgeTimer = oldAge.forcedAgeTimer;
        }
        if (params.preserveCanPickUpLoot()) {
            heir.setCanPickUpLoot(donor.canPickUpLoot());
        }
        heir.setLeftHanded(donor.isLeftHanded());
        heir.setNoAi(donor.isNoAi());
        if (donor.isPersistenceRequired()) {
            heir.setPersistenceRequired();
        }
        heir.setCustomNameVisible(donor.isCustomNameVisible());
        heir.setSharedFlagOnFire(donor.isOnFire());
        heir.setPermanentlyInvulnerable(donor.isPermanentlyInvulnerable());
        heir.setInvulnerableTime(donor.getInvulnerableTime());
        heir.setNoGravity(donor.isNoGravity());
        heir.setPortalCooldown(donor.getPortalCooldown());
        heir.setSilent(donor.isSilent());
        donor.entityTags().forEach(heir::addTag);
        if (donor.getCustomName() != null) {
            heir.setCustomName(donor.getCustomName());
        }
        if (params.team() != null) {
            var scoreboard = heir.level().getScoreboard();
            scoreboard.addPlayerToTeam(heir.getStringUUID(), params.team());
            if (donor.getTeam() == params.team()) {
                scoreboard.removePlayerFromTeam(donor.getStringUUID(), donor.getTeam());
            }
        }
        if (donor instanceof net.minecraft.world.entity.monster.zombie.Zombie oldZombie && heir instanceof net.minecraft.world.entity.monster.zombie.Zombie newZombie && oldZombie.canBreakDoors()) {
            newZombie.setCanBreakDoors(true);
        }
    }
    public boolean shouldDiscardAfterConversion() {
        return discardsOriginal();
    }
    public void convert(Mob from, Mob to, ConversionParams params) {
        Mob donor = from;
        Mob heir = to;
        if (this == SPLIT_ON_DEATH) {
            if (donor.getFirstPassenger() != null) {
                donor.getFirstPassenger().stopRiding();
            }
            if (donor.getLeashHolder() != null) {
                donor.dropLeash();
            }
            convertCommon(donor, heir, params);
            return;
        }
        Entity rider = donor.getFirstPassenger();
        heir.copyPosition(donor);
        heir.setDeltaMovement(donor.getDeltaMovement());
        if (rider != null) {
            rider.stopRiding();
            rider.boardingCooldown = 0;
            for (Entity occupant : heir.getPassengers()) {
                occupant.stopRiding();
                occupant.remove(Entity.RemovalReason.DISCARDED);
            }
            rider.startRiding(heir);
        }
        Entity mount = donor.getVehicle();
        if (mount != null) {
            donor.stopRiding();
            heir.startRiding(mount, false, false);
        }
        if (params.keepEquipment()) {
            for (EquipmentSlot slot : EquipmentSlot.VALUES) {
                ItemStack worn = donor.getItemBySlot(slot);
                if (!worn.isEmpty()) {
                    heir.setItemSlot(slot, worn.copyAndClear());
                    heir.setDropChance(slot, donor.getDropChances().byEquipment(slot));
                }
            }
        }
        heir.fallDistance = donor.fallDistance;
        heir.setSharedFlag(7, donor.isFallFlying());
        heir.lastHurtByPlayerMemoryTime = donor.lastHurtByPlayerMemoryTime;
        heir.hurtTime = donor.hurtTime;
        heir.yBodyRot = donor.yBodyRot;
        heir.setOnGround(donor.onGround());
        donor.getSleepingPos().ifPresent(heir::setSleepingPos);
        if (donor.getLeashHolder() != null) {
            heir.setLeashedTo(donor.getLeashHolder(), true);
        }
        convertCommon(donor, heir, params);

    }
}
