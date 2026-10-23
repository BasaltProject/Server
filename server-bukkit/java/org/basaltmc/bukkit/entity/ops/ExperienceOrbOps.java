package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.ExperienceOrb;

public interface ExperienceOrbOps extends ExperienceOrb, Cored {
    default int getExperience() {
        return core().nbtInt("Value");
    }

    default void setExperience(int value) {
        core().nbtSet("Value", "short", Math.max(1, Math.min(Short.MAX_VALUE, value)));
    }

    default int getCount() {
        return core().nbtInt("Count", 1);
    }

    default void setCount(int count) {
        core().nbtSet("Count", "int", Math.max(1, count));
    }
}
