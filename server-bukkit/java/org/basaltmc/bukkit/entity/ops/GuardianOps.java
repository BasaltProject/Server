package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Guardian;

public interface GuardianOps extends Guardian, Cored {
    default boolean isElder() {
        return getType() == org.bukkit.entity.EntityType.ELDER_GUARDIAN;
    }

    default boolean setLaser(boolean activated) {
        return core().test("guardian_laser", activated);
    }

    default boolean hasLaser() {
        return core().integer("guardian_laser_target") >= 0;
    }

    default int getLaserDuration() {
        return 80;
    }

    default int getLaserTicks() {
        return core().integer("guardian_laser_ticks");
    }

    default void setLaserTicks(int ticks) {
        core().set("guardian_laser_ticks", ticks);
    }

    default void setElder(boolean elder) {
        if (elder != isElder()) {
            core().retype(elder ? "elder_guardian" : "guardian");
        }
    }

    default boolean isMoving() {
        return core().flag("guardian_moving");
    }
}
