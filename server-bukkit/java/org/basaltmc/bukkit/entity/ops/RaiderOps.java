package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.Location;
import org.bukkit.entity.Raider;

public interface RaiderOps extends Raider, Cored {
    default int getWave() {
        return core().integer("raid_wave");
    }

    default void setWave(int wave) {
        core().set("raid_wave", wave);
    }

    default boolean isCanJoinRaid() {
        return core().flag("raid_can_join");
    }

    default void setCanJoinRaid(boolean join) {
        core().set("raid_can_join", join);
    }

    default boolean isPatrolLeader() {
        return core().flag("patrol_leader");
    }

    default void setPatrolLeader(boolean leader) {
        core().set("patrol_leader", leader);
    }

    default org.bukkit.block.Block getPatrolTarget() {
        Location at = MobPositions.parse(core(), core().text("patrol_target"));
        return at == null ? null : at.getBlock();
    }

    default void setPatrolTarget(org.bukkit.block.Block block) {
        core().set("patrol_target", block == null ? "" : block.getX() + "," + block.getY() + "," + block.getZ());
    }

    default int getTicksOutsideRaid() {
        return core().integer("raid_ticks_outside");
    }

    default void setTicksOutsideRaid(int ticks) {
        core().set("raid_ticks_outside", ticks);
    }

    default boolean isCelebrating() {
        return core().flag("raid_celebrating");
    }

    default void setCelebrating(boolean celebrating) {
        core().call("raider_celebrate", celebrating);
    }

    default org.bukkit.Raid getRaid() {
        int id = core().integer("raid_id");
        return id < 0 ? null : core().worldRef().getRaids().stream().filter(raid -> raid.getId() == id).findFirst()
            .orElse(null);
    }

    default org.bukkit.Sound getCelebrationSound() {
        return org.bukkit.Registry.SOUNDS.get(org.bukkit.NamespacedKey.minecraft("entity." + core().keyName()
            + ".celebrate"));
    }
}
