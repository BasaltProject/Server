package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Hoglin;

public interface HoglinOps extends Hoglin, Cored {
    default boolean isImmuneToZombification() {
        return core().nbtFlag("IsImmuneToZombification");
    }

    default void setImmuneToZombification(boolean immune) {
        core().nbtSet("IsImmuneToZombification", "bool", immune);
    }

    default boolean isAbleToBeHunted() {
        return !core().nbtFlag("CannotBeHunted");
    }

    default void setIsAbleToBeHunted(boolean able) {
        core().nbtSet("CannotBeHunted", "bool", !able);
    }
}
