package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.PigZombie;

public interface PigZombieOps extends PigZombie, Cored {
    default int getAnger() {
        long end = core().nbtValue("anger_end_time") instanceof Number number ? number.longValue() : 0L;
        return (int) Math.max(end - getWorld().getGameTime(), 0L);
    }

    default void setAnger(int ticks) {
        core().nbtSet("anger_end_time", "long", ticks <= 0 ? 0L : getWorld().getGameTime() + ticks);
    }

    default boolean isAngry() {
        return getAnger() > 0;
    }

    default void setAngry(boolean angry) {
        setAnger(angry ? 600 : 0);
    }
}
