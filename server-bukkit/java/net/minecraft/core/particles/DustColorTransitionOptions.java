// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;

import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public class DustColorTransitionOptions extends ScalableParticleOptionsBase {
    public static final int SCULK_PARTICLE_COLOR = 3790560;
    public static final DustColorTransitionOptions SCULK_TO_REDSTONE = new DustColorTransitionOptions(SCULK_PARTICLE_COLOR, DustParticleOptions.REDSTONE_PARTICLE_COLOR, 1.0F);
    public static final MapCodec<DustColorTransitionOptions> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.core.particles.DustColorTransitionOptions.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, DustColorTransitionOptions> STREAM_CODEC = StreamCodec.composite(
        ByteBufCodecs.INT, options -> options.fromColor, ByteBufCodecs.INT, options -> options.toColor,
        ByteBufCodecs.FLOAT, ScalableParticleOptionsBase::getScale, DustColorTransitionOptions::new);
    private final int fromColor;
    private final int toColor;

    public DustColorTransitionOptions(int fromColor, int toColor, float scale) {
        super(scale);
        this.fromColor = fromColor;
        this.toColor = toColor;
    }

    private static org.joml.Vector3f vector(int color) {
        return new org.joml.Vector3f((color >> 16 & 255) / 255.0F, (color >> 8 & 255) / 255.0F, (color & 255) / 255.0F);
    }

    public org.joml.Vector3f getFromColor() {
        return vector(fromColor);
    }

    public org.joml.Vector3f getToColor() {
        return vector(toColor);
    }

    public ParticleType<DustColorTransitionOptions> getType() {
        return ParticleTypes.DUST_COLOR_TRANSITION;
    }
}
