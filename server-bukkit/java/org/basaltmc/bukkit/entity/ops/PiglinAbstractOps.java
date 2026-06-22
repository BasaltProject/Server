package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.PiglinAbstract;

public interface PiglinAbstractOps extends PiglinAbstract, Cored {
    default boolean isImmuneToZombification() {
        return core().nbtFlag("IsImmuneToZombification");
    }

    default void setImmuneToZombification(boolean immune) {
        core().nbtSet("IsImmuneToZombification", "bool", immune);
    }

    default boolean isBaby() {
        return core().nbtFlag("IsBaby");
    }

    default void setBaby(boolean baby) {
        core().nbtSet("IsBaby", "bool", baby);
    }
}
