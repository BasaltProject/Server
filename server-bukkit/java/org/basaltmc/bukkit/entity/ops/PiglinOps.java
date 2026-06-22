package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Piglin;

public interface PiglinOps extends Piglin, Cored {
    default boolean isAbleToHunt() {
        return !core().nbtFlag("CannotHunt");
    }

    default void setIsAbleToHunt(boolean able) {
        core().nbtSet("CannotHunt", "bool", !able);
    }
}
