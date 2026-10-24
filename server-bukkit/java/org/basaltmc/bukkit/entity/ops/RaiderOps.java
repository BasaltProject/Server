package org.basaltmc.bukkit.entity.ops;

import org.bukkit.Location;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Raider;

public interface RaiderOps extends Raider, Cored {
    default int getWave() {
        return core().nbtInt("Wave");
    }

    default void setWave(int wave) {
        core().nbtSet("Wave", "int", wave);
    }

    default boolean isCanJoinRaid() {
        return core().nbtFlag("CanJoinRaid");
    }

    default void setCanJoinRaid(boolean join) {
        core().nbtSet("CanJoinRaid", "bool", join);
    }

    default boolean isPatrolLeader() {
        return core().nbtFlag("PatrolLeader");
    }

    default void setPatrolLeader(boolean leader) {
        core().nbtSet("PatrolLeader", "bool", leader);
    }

    default org.bukkit.block.Block getPatrolTarget() {
        Location at = EntityNbt.position(core().worldRef(), EntityNbt.compound(core(), "").get("PatrolTarget"));
        return at == null ? null : at.getBlock();
    }

    default void setPatrolTarget(org.bukkit.block.Block block) {
        if (block == null) {
            core().nbtRemove("PatrolTarget");
        } else {
            core().nbtSet("PatrolTarget", "pos", block.getX() + "," + block.getY() + "," + block.getZ());
        }
    }
}
