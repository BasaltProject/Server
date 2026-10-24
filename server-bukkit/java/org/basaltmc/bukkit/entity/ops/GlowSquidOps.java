package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.GlowSquid;

public interface GlowSquidOps extends GlowSquid, Cored {
    default int getDarkTicksRemaining() {
        return core().nbtInt("DarkTicksRemaining");
    }

    default void setDarkTicksRemaining(int ticks) {
        core().nbtSet("DarkTicksRemaining", "int", ticks);
    }
}
