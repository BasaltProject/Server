package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Zoglin;

public interface ZoglinOps extends Zoglin, Cored {
    default boolean isBaby() {
        return core().integer("breed_age") < 0;
    }

    default void setBaby(boolean baby) {
        core().set("breed_age", baby ? -24000 : 0);
    }
}
