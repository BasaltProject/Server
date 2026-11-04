// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.consume_effects;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
public record RemoveStatusEffectsConsumeEffect(HolderSet<MobEffect> effects) implements ConsumeEffect {
    public static final MapCodec<RemoveStatusEffectsConsumeEffect> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.consume_effects.RemoveStatusEffectsConsumeEffect.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, RemoveStatusEffectsConsumeEffect> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.consume_effects.RemoveStatusEffectsConsumeEffect.class, "STREAM_CODEC");
    public RemoveStatusEffectsConsumeEffect(Holder<MobEffect> only) {
        this(net.minecraft.core.HolderSet.direct(only));
    }
    public ConsumeEffect.Type<RemoveStatusEffectsConsumeEffect> getType() {
        return ConsumeEffect.Type.REMOVE_EFFECTS;
    }
    public boolean apply(Level level, ItemStack stack, LivingEntity user) {
        boolean removed = false;
        for (Holder<MobEffect> effect : effects) {
            removed |= user.removeEffect(effect);
        }
        return removed;
    }
}
