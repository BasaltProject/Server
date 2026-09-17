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

    default SpawnReason getSpawnReason() {
        String stored = core().nbtText("SpawnReason");
        try {
            return stored == null ? SpawnReason.UNKNOWN : SpawnReason.valueOf(stored);
        } catch (IllegalArgumentException unknown) {
            return SpawnReason.UNKNOWN;
        }
    }

    default java.util.UUID getSourceEntityId() {
        return uuidAt("SourceEntity");
    }

    default java.util.UUID getTriggerEntityId() {
        return uuidAt("TriggerEntity");
    }

    private java.util.UUID uuidAt(String key) {
        return org.basaltmc.bukkit.item.Convert.uuidOf(EntityNbt.compound(core(), "").get(key));
    }
}
