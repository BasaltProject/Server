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

    default org.bukkit.entity.Entity getCausingEntity() {
        return core().entity(core().integer("bolt_cause"));
    }

    default org.bukkit.entity.Player getCausingPlayer() {
        return getCausingEntity() instanceof org.bukkit.entity.Player player ? player : null;
    }

    default void setCausingPlayer(org.bukkit.entity.Player player) {
        core().set("bolt_cause", player == null ? -1 : player.getEntityId());
    }
}
