// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;

import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public class SpellParticleOption implements ParticleOptions {
    private final ParticleType<SpellParticleOption> type;
    private final int color;
    private final float power;

    public static MapCodec<SpellParticleOption> codec(ParticleType<SpellParticleOption> type) {
        return org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(SpellParticleOption.class, "CODEC");
    }

    public static StreamCodec<? super io.netty.buffer.ByteBuf, SpellParticleOption> streamCodec(ParticleType<SpellParticleOption> type) {
        return StreamCodec.of((buf, options) -> {
            buf.writeInt(options.color);
            buf.writeFloat(options.power);
        }, buf -> new SpellParticleOption(type, buf.readInt(), buf.readFloat()));
    }

    private SpellParticleOption(ParticleType<SpellParticleOption> type, int color, float power) {
        this.type = type;
        this.color = color;
        this.power = power;
    }

    public ParticleType<SpellParticleOption> getType() {
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

    public float getPower() {
        return power;
    }

    public static SpellParticleOption create(ParticleType<SpellParticleOption> type, int color, float power) {
        return new SpellParticleOption(type, color, power);
    }

    public static SpellParticleOption create(ParticleType<SpellParticleOption> type, float red, float green, float blue, float power) {
        return new SpellParticleOption(type, 0xFF000000 | channel(red) << 16 | channel(green) << 8 | channel(blue), power);
    }

    private static int channel(float value) {
        return Math.round(Math.max(0.0F, Math.min(1.0F, value)) * 255.0F);
    }
}
