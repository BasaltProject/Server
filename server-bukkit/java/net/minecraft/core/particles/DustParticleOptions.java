// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;

import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public class DustParticleOptions extends ScalableParticleOptionsBase {
    public static final int REDSTONE_PARTICLE_COLOR = 16711680;
    public static final DustParticleOptions REDSTONE = new DustParticleOptions(REDSTONE_PARTICLE_COLOR, 1.0F);
    public static final MapCodec<DustParticleOptions> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.core.particles.DustParticleOptions.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, DustParticleOptions> STREAM_CODEC = StreamCodec.composite(
        ByteBufCodecs.INT, options -> options.color, ByteBufCodecs.FLOAT, ScalableParticleOptionsBase::getScale, DustParticleOptions::new);
    private final int color;

    public DustParticleOptions(int color, float scale) {
        super(scale);
        this.color = color;
    }

    public ParticleType<DustParticleOptions> getType() {
        return ParticleTypes.DUST;
    }

    public org.joml.Vector3f getColor() {
        return new org.joml.Vector3f((color >> 16 & 255) / 255.0F, (color >> 8 & 255) / 255.0F, (color & 255) / 255.0F);
    }
}
