package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.SulfurCube;

public interface SulfurCubeOps extends SulfurCube, Cored {
    default boolean swallow(org.bukkit.inventory.ItemStack item) {
        return item != null && !item.getType().isAir()
            && core().test("sulfur_swallow", org.basaltmc.bukkit.item.Nbt.writeNamed(
                org.basaltmc.bukkit.item.Convert.stack(item)));
    }
}
