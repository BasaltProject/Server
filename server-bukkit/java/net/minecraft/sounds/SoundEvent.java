// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.sounds;

import java.util.Optional;
import net.minecraft.resources.Identifier;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
public record SoundEvent(Identifier location, Optional fixedRange) {
    public static final Codec<SoundEvent> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.sounds.SoundEvent.class, "DIRECT_CODEC");
    public static final Codec<Holder<SoundEvent>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.sounds.SoundEvent.class, "CODEC");
    public static final StreamCodec<ByteBuf, SoundEvent> DIRECT_STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.sounds.SoundEvent.class, "DIRECT_STREAM_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<SoundEvent>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.sounds.SoundEvent.class, "STREAM_CODEC");
    public static SoundEvent createVariableRangeEvent(Identifier location) {
        return new SoundEvent(location, java.util.Optional.empty());
    }
    public static SoundEvent createFixedRangeEvent(Identifier location, float range) {
        return new SoundEvent(location, java.util.Optional.of(range));
    }
    public float getRange(float volume) {
        return fixedRange.isPresent() ? (Float) fixedRange.get() : (volume > 1.0F ? 16.0F * volume : 16.0F);
    }
}
