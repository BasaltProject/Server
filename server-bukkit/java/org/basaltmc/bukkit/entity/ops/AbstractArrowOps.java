package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.AbstractArrow;

public interface AbstractArrowOps extends AbstractArrow, Cored {
    default boolean isCritical() {
        return core().flag("critical");
    }

    default void setCritical(boolean critical) {
        core().set("critical", critical);
    }

    default AbstractArrow.PickupStatus getPickupStatus() {
        return AbstractArrow.PickupStatus.values()[Math.min(2, Math.max(0, core().integer("arrow_pickup")))];
    }

    default void setPickupStatus(AbstractArrow.PickupStatus status) {
        core().set("arrow_pickup", status.ordinal());
    }

    default int getPierceLevel() {
        return core().integer("pierce_level");
    }

    default void setPierceLevel(int level) {
        core().set("pierce_level", level);
    }

    default boolean isInBlock() {
        return isOnGround();
    }
}
