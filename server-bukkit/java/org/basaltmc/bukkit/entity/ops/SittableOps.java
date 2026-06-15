package org.basaltmc.bukkit.entity.ops;

import org.bukkit.entity.Sittable;

public interface SittableOps extends Sittable, org.basaltmc.bukkit.entity.Cored {
    default boolean isSitting() {
        return core().flag("sitting");
    }

    default void setSitting(boolean sitting) {
        core().set("sitting", sitting);
    }
}
