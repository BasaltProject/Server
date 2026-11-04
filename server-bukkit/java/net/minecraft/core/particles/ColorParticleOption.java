// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;

import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public class ColorParticleOption implements ParticleOptions {
    private final ParticleType<ColorParticleOption> type;
    private final int color;

    public static MapCodec<ColorParticleOption> codec(ParticleType<ColorParticleOption> type) {
        return org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(ColorParticleOption.class, "CODEC");
    }

    public static StreamCodec<? super io.netty.buffer.ByteBuf, ColorParticleOption> streamCodec(ParticleType<ColorParticleOption> type) {
        return StreamCodec.of((buf, options) -> buf.writeInt(options.color), buf -> new ColorParticleOption(type, buf.readInt()));
    }

    private ColorParticleOption(ParticleType<ColorParticleOption> type, int color) {
        this.type = type;
        this.color = color;
    }

    public ParticleType<ColorParticleOption> getType() {
        return type;
    }

    public float getRed() {
        return (color >> 16 & 255) / 255.0F;
    }

    public float getGreen() {
        return (color >> 8 & 255) / 255.0F;
    }

    public float getBlue() {
        return (color & 255) / 255.0F;
    }

    public float getAlpha() {
        return (color >>> 24) / 255.0F;
    }

    public static ColorParticleOption create(ParticleType<ColorParticleOption> type, int color) {
        return new ColorParticleOption(type, color);
    }

    public static ColorParticleOption create(ParticleType<ColorParticleOption> type, float red, float green, float blue) {
        return new ColorParticleOption(type, 0xFF000000 | channel(red) << 16 | channel(green) << 8 | channel(blue));
    }

    private static int channel(float value) {
        return Math.round(Math.max(0.0F, Math.min(1.0F, value)) * 255.0F);
    }
}
