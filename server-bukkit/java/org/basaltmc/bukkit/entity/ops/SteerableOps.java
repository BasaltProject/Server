package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.Material;
import org.bukkit.entity.LivingEntity;
import org.bukkit.entity.Steerable;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.ItemStack;

public interface SteerableOps extends Steerable, Cored {
    default boolean hasSaddle() {
        ItemStack worn = ((LivingEntity) this).getEquipment().getItem(EquipmentSlot.SADDLE);
        return worn != null && !worn.getType().isAir();
    }

    default void setSaddle(boolean saddled) {
        ((LivingEntity) this).getEquipment().setItem(EquipmentSlot.SADDLE,
            saddled ? new ItemStack(Material.SADDLE) : null);
    }

    default Material getSteerMaterial() {
        return core().type == org.bukkit.entity.EntityType.STRIDER ? Material.WARPED_FUNGUS_ON_A_STICK
            : Material.CARROT_ON_A_STICK;
    }

    default int getBoostTicks() {
        return core().flag("boost_active") ? core().integer("boost_total") : 0;
    }

    default void setBoostTicks(int ticks) {
        if (ticks < 0) {
            throw new IllegalArgumentException("ticks must be >= 0");
        }
        core().call("boost_start", (long) ticks);
    }

    default int getCurrentBoostTicks() {
        return core().flag("boost_active") ? core().integer("boost_time") : 0;
    }

    default void setCurrentBoostTicks(int ticks) {
        if (core().flag("boost_active")) {
            int max = core().integer("boost_total");
            if (ticks < 0 || ticks > max) {
                throw new IllegalArgumentException("boost ticks must not exceed 0 or " + max + " (inclusive)");
            }
            core().set("boost_time", ticks);
        }
    }
}
