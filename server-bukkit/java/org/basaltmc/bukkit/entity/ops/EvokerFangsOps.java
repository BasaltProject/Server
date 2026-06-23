package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.EvokerFangs;

public interface EvokerFangsOps extends EvokerFangs, Cored {
    default int getAttackDelay() {
        return core().nbtInt("Warmup");
    }

    default void setAttackDelay(int delay) {
        core().nbtSet("Warmup", "int", delay);
    }

    default org.bukkit.entity.LivingEntity getOwner() {
        Object stored = EntityNbt.compound(core(), "").get("Owner");
        return stored instanceof int[] && org.bukkit.Bukkit.getEntity(org.basaltmc.bukkit.item.Convert.uuidOf(stored))
            instanceof org.bukkit.entity.LivingEntity owner ? owner : null;
    }

    default void setOwner(org.bukkit.entity.LivingEntity owner) {
        if (owner == null) {
            core().nbtRemove("Owner");
        } else {
            core().nbtSet("Owner", "uuid", owner.getUniqueId().toString());
        }
    }
}
