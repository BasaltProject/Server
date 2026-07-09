package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;

public interface HopperMinecartOps extends org.bukkit.entity.minecart.HopperMinecart, Cored {
    default org.bukkit.inventory.Inventory getInventory() {
        return ContainerEntityOps.of(core(), this);
    }

    default boolean isEnabled() {
        return core().nbtValue("Enabled") == null || core().nbtFlag("Enabled");
    }

    default void setEnabled(boolean enabled) {
        core().nbtSet("Enabled", "bool", enabled);
    }

    default int getPickupCooldown() {
        return 0;
    }

    default void setPickupCooldown(int cooldown) {
        throw org.basaltmc.bukkit.Unsupported.hit("HopperMinecart#setPickupCooldown: hopper minecarts have no pickup cooldown in this game version");
    }
}
