// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;
public interface ParticleOptions {
    default ParticleType<?> getType() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.core.particles.ParticleOptions#getType");
    }
}
