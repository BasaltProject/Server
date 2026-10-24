package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Skeleton;

public interface SkeletonOps extends Skeleton, Cored {
    default int getConversionTime() {
        return core().integer("conversion_countdown");
    }

    default void setConversionTime(int time) {
        core().set("conversion_countdown", time);
    }

    default boolean isConverting() {
        return core().integer("conversion_countdown") > -1;
    }

    default int inPowderedSnowTime() {
        return Math.max(0, core().integer("conversion_clock"));
    }
}
