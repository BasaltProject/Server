package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.DyeColor;
import org.bukkit.material.Colorable;

public interface ColorableOps extends Colorable, Cored {
    default DyeColor getColor() {
        int id = core().nbtInt("Color");
        return id >= 0 && id < DyeColor.values().length ? DyeColor.values()[id] : null;
    }

    default void setColor(DyeColor color) {
        core().nbtSet("Color", "byte", color == null ? 16 : color.ordinal());
    }
}
