package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.PigZombie;

public interface PigZombieOps extends PigZombie, Cored {
    default int getAnger() {
        long end = core().property("neutral_anger_end") instanceof Number number ? number.longValue() : 0L;
        return (int) Math.max(end - getWorld().getGameTime(), 0L);
    }

    default void setAnger(int ticks) {
        core().set("neutral_anger_end", ticks <= 0 ? -1L : getWorld().getGameTime() + ticks);
    }

    default boolean isAngry() {
        return getAnger() > 0;
    }

    default void setAngry(boolean angry) {
        setAnger(angry ? 600 : 0);
    }
}
