package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Ravager;

public interface RavagerOps extends Ravager, Cored {
    default int getAttackTicks() {
        return core().nbtInt("AttackTick");
    }

    default void setAttackTicks(int ticks) {
        core().nbtSet("AttackTick", "int", ticks);
    }

    default int getStunnedTicks() {
        return core().nbtInt("StunTick");
    }

    default void setStunnedTicks(int ticks) {
        core().nbtSet("StunTick", "int", ticks);
    }

    default int getRoarTicks() {
        return core().nbtInt("RoarTick");
    }

    default void setRoarTicks(int ticks) {
        core().nbtSet("RoarTick", "int", ticks);
    }
}
