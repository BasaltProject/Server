package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Camel;

public interface CamelOps extends Camel, Cored {
    default boolean isDashing() {
        return core().flag("horse_dashing");
    }

    default void setDashing(boolean dashing) {
        core().set("horse_dashing", dashing);
    }
}
