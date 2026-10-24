package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.LivingEntity;

public interface LivingExtraOps extends LivingEntity, Cored {
    default int getMaximumNoDamageTicks() {
        return core().property("max_no_damage_ticks") instanceof Number ticks ? ticks.intValue() : 20;
    }

    default void setMaximumNoDamageTicks(int ticks) {
        core().set("max_no_damage_ticks", (long) ticks);
    }

    default void setMaximumAir(int ticks) {
        core().set("max_air", (long) ticks);
    }

    default <T> T getMemory(org.bukkit.entity.memory.MemoryKey<T> key) {
        return LivingExtras.memory(core(), key);
    }

    default <T> void setMemory(org.bukkit.entity.memory.MemoryKey<T> key, T value) {
        if (value == null) {
            core().memories.remove(key);
        } else {
            core().memories.put(key, value);
        }
    }

    default boolean isClimbing() {
        return LivingExtras.climbing(this);
    }

    default boolean isCollidable() {
        return !Boolean.FALSE.equals(core().property("collidable"));
    }

    default void setCollidable(boolean collidable) {
        core().set("collidable", collidable);
    }

    default boolean isGliding() {
        return core().flag("gliding");
    }

    default void setGliding(boolean gliding) {
        core().set("gliding", gliding);
    }

    default boolean isSwimming() {
        return core().flag("swimming");
    }

    default void setSwimming(boolean swimming) {
        core().set("swimming", swimming);
    }

    default boolean isJumping() {
        return core().flag("jumping");
    }

    default void setJumping(boolean jumping) {
        core().set("jumping", jumping);
    }

    default boolean isRiptiding() {
        return core().flag("riptiding");
    }

    default void setRiptiding(boolean riptiding) {
        core().set("riptiding", riptiding);
    }

    default org.bukkit.block.Block getTargetBlock(int maxDistance,
            com.destroystokyo.paper.block.TargetBlockInfo.FluidMode fluids) {
        return LivingExtras.block(this, core(), maxDistance, fluids);
    }

    default org.bukkit.block.BlockFace getTargetBlockFace(int maxDistance,
            com.destroystokyo.paper.block.TargetBlockInfo.FluidMode fluids) {
        return LivingExtras.face(this, core(), maxDistance, fluids);
    }

    default com.destroystokyo.paper.block.TargetBlockInfo getTargetBlockInfo(int maxDistance,
            com.destroystokyo.paper.block.TargetBlockInfo.FluidMode fluids) {
        return LivingExtras.blockInfo(this, core(), maxDistance, fluids);
    }

    default com.destroystokyo.paper.entity.TargetEntityInfo getTargetEntityInfo(int maxDistance,
            boolean ignoreBlocks) {
        return LivingExtras.entityInfo(this, core(), maxDistance, ignoreBlocks);
    }

    default float getSoundVolume() {
        return core().property("sound_volume") instanceof Number volume ? volume.floatValue() : 1.0F;
    }

    default org.bukkit.Sound getHurtSound() {
        return LivingExtras.sound(core().call("hurt_sound"), org.bukkit.Sound.ENTITY_GENERIC_HURT);
    }

    default org.bukkit.Sound getHurtSound(org.bukkit.damage.DamageSource source) {
        return getHurtSound();
    }

    default org.bukkit.Sound getDeathSound() {
        return LivingExtras.sound(core().call("death_sound"), org.bukkit.Sound.ENTITY_GENERIC_DEATH);
    }

    default org.bukkit.Sound getFallDamageSound(int fallHeight) {
        return fallHeight > 4 ? getFallDamageSoundBig() : getFallDamageSoundSmall();
    }

    default org.bukkit.Sound getFallDamageSoundSmall() {
        return org.bukkit.Sound.ENTITY_GENERIC_SMALL_FALL;
    }

    default org.bukkit.Sound getFallDamageSoundBig() {
        return org.bukkit.Sound.ENTITY_GENERIC_BIG_FALL;
    }

    default org.bukkit.Sound getDrinkingSound(org.bukkit.inventory.ItemStack item) {
        return item != null && item.getType() == org.bukkit.Material.HONEY_BOTTLE
            ? org.bukkit.Sound.ITEM_HONEY_BOTTLE_DRINK : org.bukkit.Sound.ENTITY_GENERIC_DRINK;
    }

    default org.bukkit.Sound getEatingSound(org.bukkit.inventory.ItemStack item) {
        return org.bukkit.Sound.ENTITY_GENERIC_EAT;
    }

    default boolean hasActiveItem() {
        return core().property("active_slot") instanceof Number slot && slot.intValue() >= 0;
    }

    default org.bukkit.inventory.EquipmentSlot getActiveItemHand() {
        return LivingExtras.activeHand(core());
    }

    default org.bukkit.inventory.ItemStack getActiveItem() {
        return LivingExtras.activeItem(this, core());
    }

    default org.bukkit.inventory.ItemStack getItemInUse() {
        return hasActiveItem() ? getActiveItem() : null;
    }

    default int getActiveItemRemainingTime() {
        return core().integer("active_ticks");
    }

    default void setActiveItemRemainingTime(int ticks) {
        core().set("active_ticks", (long) ticks);
    }

    default int getActiveItemUsedTime() {
        return core().integer("active_used");
    }

    default int getItemInUseTicks() {
        return core().integer("active_used");
    }

    default void setItemInUseTicks(int ticks) {
        core().set("active_used", (long) ticks);
    }

    default void startUsingItem(org.bukkit.inventory.EquipmentSlot hand) {
        LivingExtras.startUsing(core(), hand);
    }

    default void clearActiveItem() {
        core().call("stop_using");
    }

    default void completeUsingActiveItem() {
        core().call("stop_using");
    }

    default void swingMainHand() {
        core().call("swing_hand", 0L);
    }

    default void swingOffHand() {
        core().call("swing_hand", 1L);
    }

    default void playHurtAnimation(float yaw) {
        core().call("hurt_animation", (double) yaw);
    }

    default void playPickupItemAnimation(org.bukkit.entity.Item item, int quantity) {
        core().call("pickup_animation", (long) item.getEntityId(), (long) quantity);
    }

    default void broadcastSlotBreak(org.bukkit.inventory.EquipmentSlot slot) {
        LivingExtras.slotBreak(core(), slot, null);
    }

    default void broadcastSlotBreak(org.bukkit.inventory.EquipmentSlot slot,
            java.util.Collection<org.bukkit.entity.Player> players) {
        LivingExtras.slotBreak(core(), slot, players);
    }

    default int getArrowCooldown() {
        return core().integer("arrow_cooldown");
    }

    default void setArrowCooldown(int ticks) {
        core().set("arrow_cooldown", (long) ticks);
    }

    default int getBeeStingersInBody() {
        return core().integer("bee_stingers");
    }

    default void setBeeStingersInBody(int count) {
        core().set("bee_stingers", (long) Math.max(0, count));
    }

    default int getBeeStingerCooldown() {
        return core().integer("bee_stinger_cooldown");
    }

    default void setBeeStingerCooldown(int ticks) {
        core().set("bee_stinger_cooldown", (long) ticks);
    }

    default float getBodyYaw() {
        return getYaw();
    }

    default void setBodyYaw(float yaw) {
        setRotation(yaw, getPitch());
    }

    default float getHurtDirection() {
        return (float) core().real("hurt_direction");
    }

    default void setHurtDirection(float direction) {
        core().set("hurt_direction", (double) direction);
    }

    default int getNoActionTicks() {
        return core().integer("no_action_time");
    }

    default void setNoActionTicks(int ticks) {
        core().set("no_action_time", (long) ticks);
    }

    default void setKiller(org.bukkit.entity.Player killer) {
        core().set("last_hurt_by_player", killer == null ? "" : killer.getUniqueId().toString());
    }

    default void registerAttribute(org.bukkit.attribute.Attribute attribute) {}

    default float getForwardsMovement() {
        return (float) core().real("move_forward");
    }

    default float getSidewaysMovement() {
        return (float) core().real("move_sideways");
    }

    default float getUpwardsMovement() {
        return (float) core().real("move_up");
    }
}
