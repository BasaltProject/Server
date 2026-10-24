package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Armadillo;

public interface ArmadilloOps extends Armadillo, Cored {
    default Armadillo.State getState() {
        return Armadillo.State.values()[Math.max(0, Math.min(3, core().integer("armadillo_state")))];
    }

    default void rollUp() {
        core().call("armadillo_roll_up");
    }

    default void rollOut() {
        core().call("armadillo_roll_out");
    }
}
