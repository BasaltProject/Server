package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.PufferFish;

public interface PufferFishOps extends PufferFish, Cored {
    default int getPuffState() {
        return core().integer("puff_state");
    }

    default void setPuffState(int state) {
        if (state < 0 || state > 2) {
            throw new IllegalArgumentException("Puff state must be between 0 and 2");
        }
        core().set("puff_state", state);
    }
}
