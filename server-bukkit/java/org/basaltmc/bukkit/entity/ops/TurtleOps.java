package org.basaltmc.bukkit.entity.ops;

import org.bukkit.Location;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Turtle;

public interface TurtleOps extends Turtle, Cored {
    default Location getHome() {
        return EntityNbt.position(core().worldRef(), EntityNbt.compound(core(), "").get("home_pos"));
    }

    default void setHome(Location location) {
        if (location == null) {
            core().nbtRemove("home_pos");
        } else {
            core().nbtSet("home_pos", "pos",
                location.getBlockX() + "," + location.getBlockY() + "," + location.getBlockZ());
        }
    }

    default boolean hasEgg() {
        return core().nbtFlag("has_egg");
    }

    default void setHasEgg(boolean egg) {
        core().nbtSet("has_egg", "bool", egg);
    }

    default boolean isLayingEgg() {
        return core().flag("turtle_laying_egg");
    }

    default boolean isGoingHome() {
        return core().flag("turtle_going_home");
    }
}
