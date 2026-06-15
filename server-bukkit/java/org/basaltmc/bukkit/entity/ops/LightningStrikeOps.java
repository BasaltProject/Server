package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.LightningStrike;

public interface LightningStrikeOps extends LightningStrike, Cored {
    default boolean isEffect() {
        return core().flag("bolt_visual_only");
    }

    default int getLifeTicks() {
        return core().integer("bolt_life");
    }

    default void setLifeTicks(int ticks) {
        core().set("bolt_life", ticks);
    }

    default int getFlashCount() {
        return Math.max(core().integer("bolt_flashes"), 0);
    }

    default void setFlashCount(int flashes) {
        core().set("bolt_flashes", flashes);
    }

    default int getFlashes() {
        return getFlashCount();
    }

    default void setFlashes(int flashes) {
        setFlashCount(flashes);
    }
}
