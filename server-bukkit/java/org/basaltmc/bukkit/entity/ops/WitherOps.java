package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Wither;

public interface WitherOps extends Wither, Cored {
    default int getInvulnerabilityTicks() {
        return core().nbtInt("Invul");
    }

    default void setInvulnerabilityTicks(int ticks) {
        core().nbtSet("Invul", "int", Math.max(0, ticks));
    }

    default int getInvulnerableTicks() {
        return getInvulnerabilityTicks();
    }

    default void setInvulnerableTicks(int ticks) {
        setInvulnerabilityTicks(ticks);
    }

    default void enterInvulnerabilityPhase() {
        setInvulnerabilityTicks(220);
    }

    default boolean isCharged() {
        return getHealth() <= getMaxHealth() / 2.0;
    }
}
