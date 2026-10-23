package org.basaltmc.bukkit.entity.ops;

import io.papermc.paper.entity.CollarColorable;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.DyeColor;

public interface CollarColorableOps extends CollarColorable, Cored {
    default DyeColor getCollarColor() {
        int id = core().nbtInt("CollarColor", 14);
        return DyeColor.values()[Math.floorMod(id, DyeColor.values().length)];
    }

    default void setCollarColor(DyeColor color) {
        core().nbtSet("CollarColor", "byte", color.ordinal());
    }
}
