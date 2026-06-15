package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.AbstractCubeMob;

public interface AbstractCubeMobOps extends AbstractCubeMob, Cored {
    default int getSize() {
        return core().integer("cube_size");
    }

    default void setSize(int size) {
        if (size < 1) {
            throw new IllegalArgumentException("Size must be at least 1");
        }
        core().set("cube_size", size);
    }
}
