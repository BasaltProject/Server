package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Endermite;

public interface EndermiteOps extends Endermite, Cored {
    default int getLifetimeTicks() {
        return core().nbtInt("Lifetime");
    }

    default void setLifetimeTicks(int ticks) {
        core().nbtSet("Lifetime", "int", ticks);
    }
}
