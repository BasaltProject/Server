// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;

import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.level.gameevent.PositionSource;

public class VibrationParticleOption implements ParticleOptions {
    public static final MapCodec<VibrationParticleOption> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.core.particles.VibrationParticleOption.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, VibrationParticleOption> STREAM_CODEC = StreamCodec.composite(
        PositionSource.STREAM_CODEC, VibrationParticleOption::getDestination, ByteBufCodecs.VAR_INT, VibrationParticleOption::getArrivalInTicks, VibrationParticleOption::new);
    private final PositionSource destination;
    private final int arrivalInTicks;

    public VibrationParticleOption(PositionSource destination, int arrivalInTicks) {
        this.destination = destination;
        this.arrivalInTicks = arrivalInTicks;
    }

    public ParticleType<VibrationParticleOption> getType() {
        return ParticleTypes.VIBRATION;
    }

    public PositionSource getDestination() {
        return destination;
    }

    public int getArrivalInTicks() {
        return arrivalInTicks;
    }
}
