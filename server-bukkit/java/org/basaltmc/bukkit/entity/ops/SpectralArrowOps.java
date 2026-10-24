package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.SpectralArrow;

public interface SpectralArrowOps extends SpectralArrow, Cored {
    default int getGlowingTicks() {
        return core().nbtInt("Duration", 200);
    }

    default void setGlowingTicks(int ticks) {
        core().nbtSet("Duration", "int", ticks);
    }
}
