package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Allay;

public interface AllayOps extends Allay, Cored {
    default boolean canDuplicate() {
        return core().nbtValue("DuplicationCooldown") instanceof Number cooldown ? cooldown.longValue() <= 0 : true;
    }

    default void setCanDuplicate(boolean duplicate) {
        setDuplicationCooldown(duplicate ? 0 : 6000);
    }

    default long getDuplicationCooldown() {
        return core().nbtValue("DuplicationCooldown") instanceof Number cooldown ? cooldown.longValue() : 0L;
    }

    default void setDuplicationCooldown(long cooldown) {
        core().nbtSet("DuplicationCooldown", "long", cooldown);
    }

    default void resetDuplicationCooldown() {
        setDuplicationCooldown(6000L);
    }
}
