package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.Location;
import org.bukkit.boss.DragonBattle;
import org.bukkit.entity.EnderDragon;

public interface EnderDragonOps extends EnderDragon, Cored {
    default EnderDragon.Phase getPhase() {
        return EnderDragon.Phase.values()[Math.max(0, Math.min(10, core().integer("dragon_phase")))];
    }

    default void setPhase(EnderDragon.Phase phase) {
        core().set("dragon_phase", phase.ordinal());
    }

    default DragonBattle getDragonBattle() {
        return core().worldRef().getEnderDragonBattle();
    }

    default int getDeathAnimationTicks() {
        return core().integer("dragon_death_ticks");
    }

    default Location getPodium() {
        return MobPositions.parse(core(), core().text("dragon_podium"));
    }

    default void setPodium(Location location) {
        if (location != null) {
            core().set("dragon_podium", MobPositions.format(location));
        }
    }

    default java.util.Set<org.bukkit.entity.ComplexEntityPart> getParts() {
        java.util.Set<org.bukkit.entity.ComplexEntityPart> parts = new java.util.LinkedHashSet<>();
        if (core().call("dragon_parts") instanceof String[] rows) {
            for (int index = 0; index < rows.length; index++) {
                parts.add(new org.basaltmc.bukkit.entity.BasaltDragonPart(this, core(), index));
            }
        }
        return java.util.Collections.unmodifiableSet(parts);
    }

    default org.bukkit.boss.BossBar getBossBar() {
        org.bukkit.boss.DragonBattle battle = getDragonBattle();
        return battle == null ? null : battle.getBossBar();
    }
}
