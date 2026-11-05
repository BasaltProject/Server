// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.equipment;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.item.ItemStack;
public record Equippable(EquipmentSlot slot, Holder<SoundEvent> equipSound, Optional<ResourceKey<EquipmentAsset>> assetId, Optional<Identifier> cameraOverlay, Optional<HolderSet<EntityType<?>>> allowedEntities, boolean dispensable, boolean swappable, boolean damageOnHurt, boolean equipOnInteract, boolean canBeSheared, Holder<SoundEvent> shearingSound) {
    public static final Codec<Equippable> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.equipment.Equippable.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Equippable> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.equipment.Equippable.class, "STREAM_CODEC");
    public static Equippable llamaSwag(DyeColor color) {
        return builder(EquipmentSlot.BODY).setEquipSound(net.minecraft.sounds.SoundEvents.LLAMA_SWAG)
            .setAsset(EquipmentAssets.CARPETS.get(color))
            .setAllowedEntities(net.minecraft.world.entity.EntityTypes.LLAMA, net.minecraft.world.entity.EntityTypes.TRADER_LLAMA).setCanBeSheared(true)
            .setShearingSound(net.minecraft.sounds.SoundEvents.LLAMA_CARPET_UNEQUIP).build();
    }
    public static Equippable saddle() {
        return builder(EquipmentSlot.SADDLE).setEquipSound(net.minecraft.sounds.SoundEvents.HORSE_SADDLE)
            .setAsset(EquipmentAssets.SADDLE).setAllowedEntities(entityTag(net.minecraft.tags.EntityTypeTags.CAN_EQUIP_SADDLE))
            .setEquipOnInteract(true).setCanBeSheared(true)
            .setShearingSound(net.minecraft.sounds.SoundEvents.SADDLE_UNEQUIP).build();
    }
    public static Equippable harness(DyeColor color) {
        return builder(EquipmentSlot.BODY).setEquipSound(net.minecraft.sounds.SoundEvents.HARNESS_EQUIP)
            .setAsset(EquipmentAssets.HARNESSES.get(color))
            .setAllowedEntities(entityTag(net.minecraft.tags.EntityTypeTags.CAN_EQUIP_HARNESS))
            .setEquipOnInteract(true).setCanBeSheared(true)
            .setShearingSound(net.minecraft.core.registries.BuiltInRegistries.SOUND_EVENT
                .wrapAsHolder(net.minecraft.sounds.SoundEvents.HARNESS_UNEQUIP)).build();
    }
    public InteractionResult swapWithEquipmentSlot(ItemStack inHand, Player player) {
        if (!player.canUseSlot(slot) || !canBeEquippedBy(player.typeHolder())) {
            return InteractionResult.PASS;
        }
        ItemStack inSlot = player.getItemBySlot(slot);
        boolean locked = net.minecraft.world.item.enchantment.EnchantmentHelper.hasEffect(inSlot, "minecraft:prevent_armor_change");
        if ((locked && !player.isCreative()) || ItemStack.isSameItemSameComponents(inHand, inSlot)) {
            return InteractionResult.FAIL;
        }
        if (inHand.getCount() <= 1) {
            ItemStack toHand = inSlot.isEmpty() ? inHand : inSlot.copyAndClear();
            ItemStack toSlot = player.isCreative() ? inHand.copy() : inHand.copyAndClear();
            player.setItemSlot(slot, toSlot);
            return InteractionResult.SUCCESS.heldItemTransformedTo(toHand);
        }
        ItemStack toInventory = inSlot.copyAndClear();
        player.setItemSlot(slot, inHand.consumeAndReturn(1, player));
        if (!player.getInventory().add(toInventory)) {
            player.drop(toInventory, false, null);
        }
        return InteractionResult.SUCCESS.heldItemTransformedTo(inHand);
    }
    public InteractionResult equipOnTarget(Player player, LivingEntity target, ItemStack itemStack) {
        if (!target.isEquippableInSlot(itemStack, slot) || target.hasItemInSlot(slot) || !target.isAlive()) {
            return InteractionResult.PASS;
        }
        target.setItemSlot(slot, itemStack.split(1));
        if (target instanceof net.minecraft.world.entity.Mob mob) {
            mob.setGuaranteedDrop(slot);
        }
        return InteractionResult.SUCCESS;
    }
    public boolean canBeEquippedBy(Holder<EntityType<?>> type) {
        return allowedEntities.isEmpty() || allowedEntities.get().contains(type);
    }
    private static HolderSet<EntityType<?>> entityTag(net.minecraft.tags.TagKey<EntityType<?>> tag) {
        return net.minecraft.core.registries.BuiltInRegistries.ENTITY_TYPE.get(tag).orElseThrow();
    }
    public static Builder builder(EquipmentSlot slot) {
        return new Builder(slot);
    }

    public static class Builder {
        private final EquipmentSlot slot;
        private Holder<SoundEvent> equipSound = sound("item.armor.equip_generic");
        private Optional<ResourceKey<EquipmentAsset>> assetId = Optional.empty();
        private Optional<Identifier> cameraOverlay = Optional.empty();
        private Optional<HolderSet<EntityType<?>>> allowedEntities = Optional.empty();
        private boolean dispensable = true;
        private boolean swappable = true;
        private boolean damageOnHurt = true;
        private boolean equipOnInteract;
        private boolean canBeSheared;
        private Holder<SoundEvent> shearingSound = sound("item.shears.snip");

        private static Holder<SoundEvent> sound(String id) {
            return net.minecraft.core.registries.BuiltInRegistries.SOUND_EVENT.get(Identifier.withDefaultNamespace(id)).orElseThrow();
        }

        private Builder(EquipmentSlot slot) {
            this.slot = slot;
        }

        public Builder setEquipSound(Holder<SoundEvent> equipSound) {
            this.equipSound = equipSound;
            return this;
        }

        public Builder setAsset(ResourceKey<EquipmentAsset> assetId) {
            this.assetId = Optional.of(assetId);
            return this;
        }

        public Builder setCameraOverlay(Identifier cameraOverlay) {
            this.cameraOverlay = Optional.of(cameraOverlay);
            return this;
        }

        public Builder setAllowedEntities(EntityType<?>... allowedEntities) {
            return setAllowedEntities(HolderSet.direct(EntityType::builtInRegistryHolder, allowedEntities));
        }

        public Builder setAllowedEntities(HolderSet<EntityType<?>> allowedEntities) {
            this.allowedEntities = Optional.of(allowedEntities);
            return this;
        }

        public Builder setDispensable(boolean dispensable) {
            this.dispensable = dispensable;
            return this;
        }

        public Builder setSwappable(boolean swappable) {
            this.swappable = swappable;
            return this;
        }

        public Builder setDamageOnHurt(boolean damageOnHurt) {
            this.damageOnHurt = damageOnHurt;
            return this;
        }

        public Builder setEquipOnInteract(boolean equipOnInteract) {
            this.equipOnInteract = equipOnInteract;
            return this;
        }

        public Builder setCanBeSheared(boolean canBeSheared) {
            this.canBeSheared = canBeSheared;
            return this;
        }

        public Builder setShearingSound(Holder<SoundEvent> shearingSound) {
            this.shearingSound = shearingSound;
            return this;
        }

        public Equippable build() {
            return new Equippable(slot, equipSound, assetId, cameraOverlay, allowedEntities, dispensable, swappable, damageOnHurt, equipOnInteract, canBeSheared, shearingSound);
        }
    }
}
