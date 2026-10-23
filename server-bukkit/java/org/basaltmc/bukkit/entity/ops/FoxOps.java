package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Fox;

public interface FoxOps extends Fox, Cored {
    default Fox.Type getFoxType() {
        return "snow".equals(core().nbtText("Type")) ? Fox.Type.SNOW : Fox.Type.RED;
    }

    default void setFoxType(Fox.Type type) {
        core().nbtSet("Type", "string", type == Fox.Type.SNOW ? "snow" : "red");
    }
}
