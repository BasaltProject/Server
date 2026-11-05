// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import java.util.Optional;
import net.minecraft.core.component.DataComponents;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemUtils;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.component.CustomData;
public interface Bucketable {
    boolean fromBucket();
    void setFromBucket(boolean fromBucket);
    void saveToBucketTag(ItemStack bucket);
    void loadFromBucketTag(CompoundTag tag);
    ItemStack getBucketItemStack();
    SoundEvent getPickupSound();
    static void saveDefaultDataToBucketTag(Mob entity, ItemStack bucket) {
        if (entity.hasCustomName()) {
            bucket.set(DataComponents.CUSTOM_NAME, entity.getCustomName());
        }
        CustomData.update(DataComponents.BUCKET_ENTITY_DATA, bucket, tag -> {
            putIf(tag, "NoAI", entity.isNoAi());
            putIf(tag, "Silent", entity.isSilent());
            putIf(tag, "NoGravity", entity.isNoGravity());
            putIf(tag, "Glowing", entity.hasGlowingTag());
            putIf(tag, "Invulnerable", entity.isPermanentlyInvulnerable());
            putIf(tag, "PersistenceRequired", entity.isPersistenceRequired());
            tag.putFloat("Health", entity.getHealth());
        });
    }
    private static void putIf(CompoundTag tag, String key, boolean value) {
        if (value) {
            tag.putBoolean(key, true);
        }
    }
    static void loadDefaultDataFromBucketTag(Mob entity, CompoundTag tag) {
        tag.getBoolean("NoAI").ifPresent(entity::setNoAi);
        tag.getBoolean("Silent").ifPresent(entity::setSilent);
        tag.getBoolean("NoGravity").ifPresent(entity::setNoGravity);
        tag.getBoolean("Glowing").ifPresent(entity::setGlowingTag);
        tag.getBoolean("Invulnerable").ifPresent(entity::setPermanentlyInvulnerable);
        if (tag.getBoolean("PersistenceRequired").orElse(false)) {
            entity.setPersistenceRequired();
        }
        tag.getFloat("Health").ifPresent(entity::setHealth);
    }
    static <T extends LivingEntity & Bucketable> Optional<InteractionResult> bucketMobPickup(Player player, InteractionHand hand, T pickupEntity) {
        ItemStack held = player.getItemInHand(hand);
        if (!pickupEntity.canBePickedUpWithBucket(held) || !pickupEntity.isAlive()) {
            return Optional.empty();
        }
        pickupEntity.playSound(pickupEntity.getPickupSound(), 1.0F, 1.0F);
        ItemStack filled = pickupEntity.getBucketItemStack();
        pickupEntity.saveToBucketTag(filled);
        player.setItemInHand(hand, ItemUtils.createFilledResult(held, player, filled, false));
        if (!pickupEntity.level().isClientSide() && player instanceof ServerPlayer serverPlayer) {
            net.minecraft.advancements.triggers.CriteriaTriggers.FILLED_BUCKET.trigger(serverPlayer, filled);
        }
        if (pickupEntity instanceof Leashable leashable) {
            leashable.dropLeash();
        }
        pickupEntity.discard();
        return Optional.of(InteractionResult.SUCCESS);
    }
    default boolean canBePickedUpWithBucket(ItemStack itemStack) {
        return itemStack.getItem() == Items.WATER_BUCKET;
    }
}
