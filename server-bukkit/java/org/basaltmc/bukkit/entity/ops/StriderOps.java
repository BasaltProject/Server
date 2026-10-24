package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;

public interface StriderOps extends org.bukkit.entity.Strider, Cored {
    default boolean isShivering() {
        return core().flag("strider_shivering");
    }

    default void setShivering(boolean shivering) {
        core().set("strider_shivering", shivering);
    }
}
