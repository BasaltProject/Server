// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.damagesource.DamageType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
public record BlocksAttacks(float blockDelaySeconds, float disableCooldownScale, List<DamageReduction> damageReductions, ItemDamageFunction itemDamage, Optional<HolderSet<DamageType>> bypassedBy, Optional<Holder<SoundEvent>> blockSound, Optional<Holder<SoundEvent>> disableSound) {
    public static final Codec<BlocksAttacks> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.BlocksAttacks.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, BlocksAttacks> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.BlocksAttacks.class, "STREAM_CODEC");
    public void onBlocked(ServerLevel level, LivingEntity user) {
        blockSound.ifPresent(event -> level.playSound(null, user.getX(), user.getY(), user.getZ(), event, user.getSoundSource(), 1.0F, 0.8F + level.getRandom().nextFloat() * 0.4F));
    }
    public void disable(ServerLevel level, LivingEntity user, float baseSeconds, ItemStack blockingWith) {
        float seconds = baseSeconds * disableCooldownScale;
        int cooldownTicks = seconds > 0.0F ? Math.round(seconds * 20.0F) : 0;
        if (cooldownTicks <= 0) {
            return;
        }
        if (user instanceof net.minecraft.world.entity.player.Player player) {
            player.getCooldowns().addCooldown(blockingWith, cooldownTicks);
        }
        user.stopUsingItem();
        disableSound.ifPresent(event -> level.playSound(null, user.getX(), user.getY(), user.getZ(), event, user.getSoundSource(), 0.8F, 0.8F + level.getRandom().nextFloat() * 0.4F));
    }
    public void hurtBlockingItem(Level level, ItemStack item, LivingEntity user, InteractionHand hand, float damage) {
        if (!(user instanceof net.minecraft.world.entity.player.Player player)) {
            return;
        }
        if (!level.isClientSide()) {
            player.awardStat(net.minecraft.stats.Stats.ITEM_USED.get(item.getItem()));
        }
        int damageDealt = itemDamage.apply(damage);
        if (damageDealt > 0) {
            item.hurtAndBreak(damageDealt, user, hand.asEquipmentSlot());
        }
    }
    public int blockDelayTicks() {
        return Math.round(blockDelaySeconds * 20.0F);
    }
    public float resolveBlockedDamage(DamageSource source, float dealtDamage, double angle) {
        float blocked = 0.0F;
        for (DamageReduction reduction : damageReductions) {
            blocked += reduction.resolve(source, dealtDamage, angle);
        }
        return net.minecraft.util.Mth.clamp(blocked, 0.0F, dealtDamage);
    }
    public static record DamageReduction(float horizontalBlockingAngle, Optional<HolderSet<DamageType>> type, float base, float factor) {
        public static final Codec<DamageReduction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.BlocksAttacks.DamageReduction.class, "CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, DamageReduction> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.BlocksAttacks.DamageReduction.class, "STREAM_CODEC");
        public float resolve(DamageSource source, float dealtDamage, double angle) {
            if (angle > Math.toRadians(horizontalBlockingAngle)) {
                return 0.0F;
            }
            if (type.isPresent() && !org.basaltmc.bukkit.facade.HolderSets.contains(type.get(), source.typeHolder())) {
                return 0.0F;
            }
            return net.minecraft.util.Mth.clamp(base + factor * dealtDamage, 0.0F, dealtDamage);
        }
    }
    public static record ItemDamageFunction(float threshold, float base, float factor) {
        public static final Codec<ItemDamageFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.BlocksAttacks.ItemDamageFunction.class, "CODEC");
        public static final StreamCodec<ByteBuf, ItemDamageFunction> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.BlocksAttacks.ItemDamageFunction.class, "STREAM_CODEC");
        public static final ItemDamageFunction DEFAULT = new ItemDamageFunction(1.0F, 0.0F, 1.0F);
        public int apply(float dealtDamage) {
            return dealtDamage < threshold ? 0 : net.minecraft.util.Mth.floor(base + factor * dealtDamage);
        }
    }
}
