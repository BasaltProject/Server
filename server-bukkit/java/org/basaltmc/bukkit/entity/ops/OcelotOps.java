package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Ocelot;

public interface OcelotOps extends Ocelot, Cored {
    default boolean isTrusting() {
        return core().nbtFlag("Trusting");
    }

    default void setTrusting(boolean trust) {
        core().nbtSet("Trusting", "bool", trust);
    }
}
