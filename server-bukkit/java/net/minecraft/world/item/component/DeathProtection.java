// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import java.util.List;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.consume_effects.ConsumeEffect;
public record DeathProtection(List<ConsumeEffect> deathEffects) {
    public static final Codec<DeathProtection> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.DeathProtection.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, DeathProtection> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.DeathProtection.class, "STREAM_CODEC");
    public static final DeathProtection TOTEM_OF_UNDYING = new DeathProtection(java.util.List.of(
        new net.minecraft.world.item.consume_effects.ClearAllStatusEffectsConsumeEffect(),
        new net.minecraft.world.item.consume_effects.ApplyStatusEffectsConsumeEffect(java.util.List.of(
            new net.minecraft.world.effect.MobEffectInstance(net.minecraft.world.effect.MobEffects.REGENERATION, 900, 1),
            new net.minecraft.world.effect.MobEffectInstance(net.minecraft.world.effect.MobEffects.ABSORPTION, 100, 1),
            new net.minecraft.world.effect.MobEffectInstance(net.minecraft.world.effect.MobEffects.FIRE_RESISTANCE, 800, 0)))));
    public void applyEffects(ItemStack itemStack, LivingEntity entity) {
        for (net.minecraft.world.item.consume_effects.ConsumeEffect effect : deathEffects) {
            effect.apply(entity.level(), itemStack, entity);
        }
    }
}
