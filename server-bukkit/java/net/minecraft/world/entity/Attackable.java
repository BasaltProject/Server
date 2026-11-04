// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
public interface Attackable {
    default LivingEntity getLastAttacker() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Attackable#getLastAttacker");
    }
}
