package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Tadpole;

public interface TadpoleOps extends Tadpole, Cored {
    default int getAge() {
        return core().nbtInt("Age");
    }

    default void setAge(int age) {
        core().nbtSet("Age", "int", age);
    }
}
