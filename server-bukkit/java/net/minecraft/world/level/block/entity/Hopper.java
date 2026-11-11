// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;
import net.minecraft.world.Container;
import net.minecraft.world.phys.AABB;
public interface Hopper extends Container {
    AABB SUCK_AABB = new AABB(0.0, 0.6875, 0.0, 1.0, 2.0, 1.0);
    default AABB getSuckAabb() {
        return SUCK_AABB;
    }
    double getLevelX();
    double getLevelY();
    double getLevelZ();
    boolean isGridAligned();
}
