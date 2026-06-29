package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.Location;
import org.bukkit.entity.Dolphin;

public interface DolphinOps extends Dolphin, Cored {
    default int getMoistness() {
        return core().integer("moistness");
    }

    default void setMoistness(int moistness) {
        core().set("moistness", moistness);
    }

    default boolean hasFish() {
        return core().flag("got_fish");
    }

    default void setHasFish(boolean fish) {
        core().set("got_fish", fish);
    }

    default Location getTreasureLocation() {
        return MobPositions.parse(core(), core().text("dolphin_treasure"));
    }

    default void setTreasureLocation(Location location) {
        core().set("dolphin_treasure", location == null ? "" : MobPositions.format(location));
    }
}
