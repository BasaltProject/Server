// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;

import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public record SculkChargeParticleOptions(float roll) implements ParticleOptions {
    public static final MapCodec<SculkChargeParticleOptions> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.core.particles.SculkChargeParticleOptions.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, SculkChargeParticleOptions> STREAM_CODEC = StreamCodec.composite(
        ByteBufCodecs.FLOAT, SculkChargeParticleOptions::roll, SculkChargeParticleOptions::new);

    public ParticleType<SculkChargeParticleOptions> getType() {
        return ParticleTypes.SCULK_CHARGE;
    }
}
