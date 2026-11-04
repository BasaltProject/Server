// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;

import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public record GeyserBaseParticleOptions(ParticleType<GeyserBaseParticleOptions> type, int waterBlocks, float burstImpulseBase) implements ParticleOptions {
    public static MapCodec<GeyserBaseParticleOptions> codec(ParticleType<GeyserBaseParticleOptions> type) {
        return org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(GeyserBaseParticleOptions.class, "CODEC");
    }

    public static StreamCodec<? super io.netty.buffer.ByteBuf, GeyserBaseParticleOptions> streamCodec(ParticleType<GeyserBaseParticleOptions> type) {
        return StreamCodec.of((buf, options) -> {
            buf.writeInt(options.waterBlocks);
            buf.writeFloat(options.burstImpulseBase);
        }, buf -> new GeyserBaseParticleOptions(type, buf.readInt(), buf.readFloat()));
    }

    public ParticleType<GeyserBaseParticleOptions> getType() {
        return type;
    }
}
