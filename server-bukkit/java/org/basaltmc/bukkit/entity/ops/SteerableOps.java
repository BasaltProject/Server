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
}
