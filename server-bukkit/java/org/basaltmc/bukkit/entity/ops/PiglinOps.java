package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Piglin;

public interface PiglinOps extends Piglin, Cored {
    default boolean isAbleToHunt() {
        return !core().flag("piglin_cannot_hunt");
    }

    default void setIsAbleToHunt(boolean able) {
        core().set("piglin_cannot_hunt", !able);
    }

    default boolean isChargingCrossbow() {
        return core().flag("piglin_charging");
    }

    default void setChargingCrossbow(boolean charging) {
        core().set("piglin_charging", charging);
    }

    default boolean isDancing() {
        return core().flag("piglin_dancing");
    }

    default void setDancing(boolean dancing) {
        core().set("piglin_dancing", dancing);
    }
}
