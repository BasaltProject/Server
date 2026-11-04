// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.consume_effects;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
public record ApplyStatusEffectsConsumeEffect(List<MobEffectInstance> effects, float probability) implements ConsumeEffect {
    public static final MapCodec<ApplyStatusEffectsConsumeEffect> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.consume_effects.ApplyStatusEffectsConsumeEffect.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, ApplyStatusEffectsConsumeEffect> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.consume_effects.ApplyStatusEffectsConsumeEffect.class, "STREAM_CODEC");
    public ApplyStatusEffectsConsumeEffect(MobEffectInstance effect, float probability) {
        this(List.of(effect), probability);
    }
    public ApplyStatusEffectsConsumeEffect(List<MobEffectInstance> effects) {
        this(effects, 1.0F);
    }
    public ApplyStatusEffectsConsumeEffect(MobEffectInstance effect) {
        this(List.of(effect), 1.0F);
    }
    public ConsumeEffect.Type<ApplyStatusEffectsConsumeEffect> getType() {
        return ConsumeEffect.Type.APPLY_EFFECTS;
    }
    public boolean apply(Level level, ItemStack stack, LivingEntity user) {
        if (user.getRandom().nextFloat() >= probability) {
            return false;
        }
        boolean applied = false;
        for (MobEffectInstance effect : effects) {
            applied |= user.addEffect(new MobEffectInstance(effect));
        }
        return applied;
    }
}
