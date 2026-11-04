// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;

import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;

public class SimpleParticleType extends ParticleType<SimpleParticleType> implements ParticleOptions {
    protected SimpleParticleType(boolean overrideLimiter) {
        super(overrideLimiter);
    }

    public SimpleParticleType getType() {
        return this;
    }

    public MapCodec<SimpleParticleType> codec() {
        return MapCodec.unit(this);
    }

    public StreamCodec<RegistryFriendlyByteBuf, SimpleParticleType> streamCodec() {
        return StreamCodec.unit(this);
    }
}
