// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemUseAnimation;
import net.minecraft.world.item.consume_effects.ConsumeEffect;
import net.minecraft.world.level.Level;
public record Consumable(float consumeSeconds, ItemUseAnimation animation, Holder<SoundEvent> sound, boolean hasConsumeParticles, List<ConsumeEffect> onConsumeEffects) {
    public static final float DEFAULT_CONSUME_SECONDS = 1.6F;
    public static final Codec<Consumable> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.Consumable.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Consumable> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.Consumable.class, "STREAM_CODEC");
    public InteractionResult startConsuming(LivingEntity user, ItemStack stack, InteractionHand hand) {
        return canConsume(user, stack) ? InteractionResult.CONSUME : InteractionResult.FAIL;
    }
    public ItemStack onConsume(Level level, LivingEntity user, ItemStack stack) {
        for (ConsumeEffect effect : onConsumeEffects) {
            effect.apply(level, stack, user);
        }
        stack.consume(1, user);
        return stack;
    }
    public boolean canConsume(LivingEntity user, ItemStack stack) {
        return true;
    }
    public int consumeTicks() {
        return (int) (consumeSeconds * 20.0F);
    }
    public void emitParticlesAndSounds(RandomSource random, LivingEntity user, ItemStack itemStack, int particleCount) {
        if (sound != null && user.level() != null) {
            user.level().playSound(null, user.getX(), user.getY(), user.getZ(), sound.value(), net.minecraft.sounds.SoundSource.NEUTRAL, 1.0F, 1.0F);
        }
    }
    public boolean shouldEmitParticlesAndSounds(int useItemRemainingTicks) {
        int elapsed = consumeTicks() - useItemRemainingTicks;
        int threshold = (int) (consumeTicks() * 0.21875F);
        return elapsed > threshold && useItemRemainingTicks % 4 == 0;
    }
    public static Builder builder() {
        return new Builder();
    }
    public static class Builder {
        private float consumeSeconds = DEFAULT_CONSUME_SECONDS;
        private ItemUseAnimation animation = ItemUseAnimation.EAT;
        private Holder<SoundEvent> sound = net.minecraft.sounds.SoundEvents.GENERIC_EAT;
        private boolean hasConsumeParticles = true;
        private final List<ConsumeEffect> effects = new java.util.ArrayList<>();

        private Builder() {}
        public Builder consumeSeconds(float consumeSeconds) {
            this.consumeSeconds = consumeSeconds;
            return this;
        }
        public Builder animation(ItemUseAnimation animation) {
            this.animation = animation;
            return this;
        }
        public Builder sound(Holder<SoundEvent> sound) {
            this.sound = sound;
            return this;
        }
        public Builder soundAfterConsume(Holder<SoundEvent> soundAfterConsume) {
            return this;
        }
        public Builder hasConsumeParticles(boolean hasConsumeParticles) {
            this.hasConsumeParticles = hasConsumeParticles;
            return this;
        }
        public Builder onConsume(ConsumeEffect effect) {
            effects.add(effect);
            return this;
        }
        public Consumable build() {
            return new Consumable(consumeSeconds, animation, sound, hasConsumeParticles, List.copyOf(effects));
        }
    }
    public interface OverrideConsumeSound {
        SoundEvent getConsumeSound(ItemStack itemStack);
    }
}
