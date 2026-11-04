// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;

import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;

public abstract class ParticleType<T extends ParticleOptions> {
    private final boolean overrideLimiter;
    private String key = "";

    protected ParticleType(boolean overrideLimiter) {
        this.overrideLimiter = overrideLimiter;
    }

    public boolean getOverrideLimiter() {
        return overrideLimiter;
    }

    public abstract MapCodec<T> codec();

    public abstract StreamCodec<? super RegistryFriendlyByteBuf, T> streamCodec();

    public ParticleType<T> named(String key) {
        this.key = key;
        return this;
    }

    public String key() {
        return key;
    }

    @Override
    public String toString() {
        return "minecraft:" + key;
    }
}
