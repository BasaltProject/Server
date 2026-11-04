// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.consume_effects;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
public interface ConsumeEffect {
    Codec<ConsumeEffect> CODEC = org.basaltmc.bukkit.facade.DispatchCodecs.create(java.util.Map.of(
        "apply_effects", ApplyStatusEffectsConsumeEffect.class, "remove_effects", RemoveStatusEffectsConsumeEffect.class,
        "clear_all_effects", ClearAllStatusEffectsConsumeEffect.class, "teleport_randomly", TeleportRandomlyConsumeEffect.class,
        "play_sound", PlaySoundConsumeEffect.class));
    StreamCodec<RegistryFriendlyByteBuf, ConsumeEffect> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.consume_effects.ConsumeEffect.class, "STREAM_CODEC");
    Type<? extends ConsumeEffect> getType();
    boolean apply(Level level, ItemStack stack, LivingEntity user);
    public static record Type<T extends ConsumeEffect>(MapCodec<T> codec, StreamCodec<RegistryFriendlyByteBuf, T> streamCodec) {
        public static final Type<ApplyStatusEffectsConsumeEffect> APPLY_EFFECTS = new Type<>(ApplyStatusEffectsConsumeEffect.CODEC, ApplyStatusEffectsConsumeEffect.STREAM_CODEC);
        public static final Type<RemoveStatusEffectsConsumeEffect> REMOVE_EFFECTS = new Type<>(RemoveStatusEffectsConsumeEffect.CODEC, RemoveStatusEffectsConsumeEffect.STREAM_CODEC);
        public static final Type<ClearAllStatusEffectsConsumeEffect> CLEAR_ALL_EFFECTS = new Type<>(ClearAllStatusEffectsConsumeEffect.CODEC, ClearAllStatusEffectsConsumeEffect.STREAM_CODEC);
        public static final Type<TeleportRandomlyConsumeEffect> TELEPORT_RANDOMLY = new Type<>(TeleportRandomlyConsumeEffect.CODEC, TeleportRandomlyConsumeEffect.STREAM_CODEC);
        public static final Type<PlaySoundConsumeEffect> PLAY_SOUND = new Type<>(PlaySoundConsumeEffect.CODEC, PlaySoundConsumeEffect.STREAM_CODEC);
    }
}
