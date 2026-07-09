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

    default org.bukkit.entity.LivingEntity getTarget(Wither.Head head) {
        return core().call("wither_target", (long) head.ordinal()) instanceof Number id && id.intValue() >= 0
            && core().entity(id.intValue()) instanceof org.bukkit.entity.LivingEntity living ? living : null;
    }

    default void setTarget(Wither.Head head, org.bukkit.entity.LivingEntity target) {
        core().call("wither_set_target", (long) head.ordinal(), target == null ? -1L : (long) target.getEntityId());
    }

    default boolean canTravelThroughPortals() {
        return core().flag("wither_portals");
    }

    default void setCanTravelThroughPortals(boolean value) {
        core().set("wither_portals", value);
    }

    default org.bukkit.boss.BossBar getBossBar() {
        return org.basaltmc.bukkit.misc.EntityBossBar.of(core());
    }
}
