package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.AbstractSkeleton;

public interface AbstractSkeletonOps extends AbstractSkeleton, Cored {
    default boolean shouldBurnInDay() {
        return core().flag("burns_in_day");
    }

    default void setShouldBurnInDay(boolean burn) {
        core().set("burns_in_day", burn);
    }
}
