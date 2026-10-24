package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Horse;

public interface HorseOps extends Horse, Cored {
    default Horse.Color getColor() {
        return Horse.Color.values()[Math.floorMod(core().nbtInt("Variant") & 0xFF, Horse.Color.values().length)];
    }

    default void setColor(Horse.Color color) {
        core().nbtSet("Variant", "int", color.ordinal() | (getStyle().ordinal() << 8));
    }

    default Horse.Style getStyle() {
        return Horse.Style.values()[Math.floorMod((core().nbtInt("Variant") >> 8) & 0xFF, Horse.Style.values().length)];
    }

    default void setStyle(Horse.Style style) {
        core().nbtSet("Variant", "int", getColor().ordinal() | (style.ordinal() << 8));
    }

    default boolean isCarryingChest() {
        return false;
    }

    default void setCarryingChest(boolean chest) {
        if (chest) {
            throw org.basaltmc.bukkit.Unsupported.hit("Horse#setCarryingChest: a horse cannot carry a chest, a donkey or mule can");
        }
    }
}
