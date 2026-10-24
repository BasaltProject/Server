package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Hoglin;

public interface HoglinOps extends Hoglin, Cored {
    default boolean isImmuneToZombification() {
        return core().flag("immune_to_zombification");
    }

    default void setImmuneToZombification(boolean immune) {
        core().set("immune_to_zombification", immune);
    }

    default boolean isAbleToBeHunted() {
        return !core().nbtFlag("CannotBeHunted");
    }

    default void setIsAbleToBeHunted(boolean able) {
        core().nbtSet("CannotBeHunted", "bool", !able);
    }

    default int getConversionTime() {
        if (!isConverting()) {
            throw new IllegalStateException("Entity is not in the process of converting");
        }
        return core().integer("conversion_clock");
    }

    default void setConversionTime(int time) {
        core().set("conversion_clock", Math.max(0, time));
    }

    default boolean isConverting() {
        return core().test("converting");
    }
}
