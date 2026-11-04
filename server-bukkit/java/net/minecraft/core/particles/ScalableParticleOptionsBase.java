// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;

public abstract class ScalableParticleOptionsBase implements ParticleOptions {
    public static final float MIN_SCALE = 0.01F;
    public static final float MAX_SCALE = 4.0F;
    protected static final com.mojang.serialization.Codec<Float> SCALE = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.core.particles.ScalableParticleOptionsBase.class, "SCALE");
    protected final float scale;

    public ScalableParticleOptionsBase(float scale) {
        this.scale = Math.max(MIN_SCALE, Math.min(MAX_SCALE, scale));
    }

    public float getScale() {
        return scale;
    }
}
