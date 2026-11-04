// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;

import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public class PowerParticleOption implements ParticleOptions {
    private final ParticleType<PowerParticleOption> type;
    private final float power;

    public static MapCodec<PowerParticleOption> codec(ParticleType<PowerParticleOption> type) {
        return org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(PowerParticleOption.class, "CODEC");
    }

    public static StreamCodec<? super io.netty.buffer.ByteBuf, PowerParticleOption> streamCodec(ParticleType<PowerParticleOption> type) {
        return StreamCodec.of((buf, options) -> buf.writeFloat(options.power), buf -> new PowerParticleOption(type, buf.readFloat()));
    }

    private PowerParticleOption(ParticleType<PowerParticleOption> type, float power) {
        this.type = type;
        this.power = power;
    }

    public ParticleType<PowerParticleOption> getType() {
        return type;
    }

    public float getPower() {
        return power;
    }

    public static PowerParticleOption create(ParticleType<PowerParticleOption> type, float power) {
        return new PowerParticleOption(type, power);
    }
}
