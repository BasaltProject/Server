package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.PiglinAbstract;

public interface PiglinAbstractOps extends PiglinAbstract, Cored {
    default boolean isImmuneToZombification() {
        return core().flag("immune_to_zombification");
    }

    default void setImmuneToZombification(boolean immune) {
        core().set("immune_to_zombification", immune);
    }

    default boolean isBaby() {
        return core().integer("breed_age") < 0;
    }

    default void setBaby(boolean baby) {
        core().set("breed_age", baby ? -24000 : 0);
    }

    default int getConversionTime() {
        if (!isConverting()) {
            throw new IllegalStateException("Entity is not in the process of converting");
        }
        return core().integer("conversion_clock");
    }

    default void setConversionTime(int time) {
        if (time < 0) {
            core().set("conversion_clock", 0);
            core().set("immune_to_zombification", false);
        } else {
            core().set("conversion_clock", time);
        }
    }

    default boolean isConverting() {
        return core().test("converting");
    }
}
