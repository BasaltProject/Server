// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;

import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public class ShriekParticleOption implements ParticleOptions {
    public static final MapCodec<ShriekParticleOption> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.core.particles.ShriekParticleOption.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, ShriekParticleOption> STREAM_CODEC = StreamCodec.composite(
        ByteBufCodecs.VAR_INT, ShriekParticleOption::getDelay, ShriekParticleOption::new);
    private final int delay;

    public ShriekParticleOption(int delay) {
        this.delay = delay;
    }

    public ParticleType<ShriekParticleOption> getType() {
        return ParticleTypes.SHRIEK;
    }

    public int getDelay() {
        return delay;
    }
}
