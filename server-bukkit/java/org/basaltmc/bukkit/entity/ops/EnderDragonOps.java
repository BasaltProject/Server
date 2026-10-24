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
}
