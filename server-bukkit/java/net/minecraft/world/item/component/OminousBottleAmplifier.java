// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import java.util.function.Consumer;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.TooltipFlag;
import net.minecraft.world.level.Level;
public record OminousBottleAmplifier(int value) implements ConsumableListener, TooltipProvider {
    public static final int EFFECT_DURATION = 120000;
    public static final int MIN_AMPLIFIER = 0;
    public static final int MAX_AMPLIFIER = 4;
    public static final Codec<OminousBottleAmplifier> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.OminousBottleAmplifier.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, OminousBottleAmplifier> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.OminousBottleAmplifier.class, "STREAM_CODEC");
    public void onConsume(Level level, LivingEntity user, ItemStack stack, Consumable consumable) {
        user.addEffect(new net.minecraft.world.effect.MobEffectInstance(net.minecraft.world.effect.MobEffects.BAD_OMEN, EFFECT_DURATION, value, false, false, true));
    }
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        java.util.List<net.minecraft.world.effect.MobEffectInstance> effects = java.util.List.of(
            new net.minecraft.world.effect.MobEffectInstance(net.minecraft.world.effect.MobEffects.BAD_OMEN, EFFECT_DURATION, value, false, false, true));
        net.minecraft.world.item.alchemy.PotionContents.addPotionTooltip(effects, consumer, 1.0F, context.tickRate());
    }
}
