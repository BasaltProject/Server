package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Skeleton;

public interface SkeletonOps extends Skeleton, Cored {
    default int getConversionTime() {
        return core().nbtInt("StrayConversionTime", -1);
    }

    default void setConversionTime(int time) {
        core().nbtSet("StrayConversionTime", "int", time);
    }
}
