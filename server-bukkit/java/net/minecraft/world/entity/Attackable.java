// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
public interface Attackable {
    default LivingEntity getLastAttacker() {
        return null;
    }
}
