// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.consume_effects;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
public record PlaySoundConsumeEffect(Holder<SoundEvent> sound) implements ConsumeEffect {
    public static final MapCodec<PlaySoundConsumeEffect> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.consume_effects.PlaySoundConsumeEffect.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, PlaySoundConsumeEffect> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.consume_effects.PlaySoundConsumeEffect.class, "STREAM_CODEC");
    public ConsumeEffect.Type<PlaySoundConsumeEffect> getType() {
        return ConsumeEffect.Type.PLAY_SOUND;
    }
    public boolean apply(Level level, ItemStack stack, LivingEntity user) {
        level.playSound(null, user.blockPosition(), sound.value(), user.getSoundSource(), 1.0F, 1.0F);
        return true;
    }
}
