// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;

import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public record GeyserParticleOptions(ParticleType<GeyserParticleOptions> type, int waterBlocks) implements ParticleOptions {
    public static MapCodec<GeyserParticleOptions> codec(ParticleType<GeyserParticleOptions> type) {
        return org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(GeyserParticleOptions.class, "CODEC");
    }

    public static StreamCodec<? super io.netty.buffer.ByteBuf, GeyserParticleOptions> streamCodec(ParticleType<GeyserParticleOptions> type) {
        return StreamCodec.of((buf, options) -> buf.writeInt(options.waterBlocks), buf -> new GeyserParticleOptions(type, buf.readInt()));
    }

    public ParticleType<GeyserParticleOptions> getType() {
        return type;
    }
}
