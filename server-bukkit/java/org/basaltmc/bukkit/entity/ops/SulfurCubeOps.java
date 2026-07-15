package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.SulfurCube;

public interface SulfurCubeOps extends SulfurCube, Cored {
    default boolean swallow(org.bukkit.inventory.ItemStack item) {
        return item != null && !item.getType().isAir()
            && core().test("sulfur_swallow", org.basaltmc.bukkit.item.Nbt.writeNamed(
                org.basaltmc.bukkit.item.Convert.stack(item)));
    }

    default int getFuseTicks() {
        return core().integer("fuse");
    }

    default void setFuseTicks(int ticks) {
        core().set("fuse", ticks);
    }

    default boolean canExplode() {
        return core().test("sulfur_can_explode");
    }

    default boolean ignite(boolean imminent) {
        return core().test("sulfur_prime", imminent);
    }

    default boolean ignite() {
        return ignite(false);
    }
}
