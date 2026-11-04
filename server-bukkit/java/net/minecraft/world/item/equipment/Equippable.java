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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.equipment.Equippable#llamaSwag");
    }
    public static Equippable saddle() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.equipment.Equippable#saddle");
    }
    public static Equippable harness(DyeColor color) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.equipment.Equippable#harness");
    }
    public InteractionResult swapWithEquipmentSlot(ItemStack inHand, Player player) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.equipment.Equippable#swapWithEquipmentSlot");
    }
    public InteractionResult equipOnTarget(Player player, LivingEntity target, ItemStack itemStack) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.equipment.Equippable#equipOnTarget");
    }
    public boolean canBeEquippedBy(Holder<EntityType<?>> type) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.equipment.Equippable#canBeEquippedBy");
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
