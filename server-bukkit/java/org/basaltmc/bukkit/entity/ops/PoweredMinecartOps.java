package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.minecart.PoweredMinecart;

public interface PoweredMinecartOps extends PoweredMinecart, Cored {
    default int getFuel() {
        return core().integer("cart_fuel");
    }

    default void setFuel(int fuel) {
        core().set("cart_fuel", Math.max(0, fuel));
    }

    default double getPushX() {
        return core().real("cart_push_x");
    }

    default void setPushX(double x) {
        core().set("cart_push_x", x);
    }

    default double getPushZ() {
        return core().real("cart_push_z");
    }

    default void setPushZ(double z) {
        core().set("cart_push_z", z);
    }
}
