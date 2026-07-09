package org.basaltmc.bukkit.entity.ops;

import org.bukkit.Location;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.WanderingTrader;

public interface WanderingTraderOps extends WanderingTrader, Cored {
    default int getDespawnDelay() {
        return core().integer("despawn_delay");
    }

    default void setDespawnDelay(int delay) {
        core().set("despawn_delay", delay);
    }

    default void resetOffers() {
        core().call("villager_reset_offers");
    }

    default Location getWanderingTowards() {
        return MobPositions.parse(core(), core().text("wander_target"));
    }

    default void setWanderingTowards(Location location) {
        core().set("wander_target", location == null ? "" : MobPositions.format(location));
    }

    default boolean canDrinkPotion() {
        return !core().nbtFlag("Paper.CanDrinkPotionDisabled");
    }

    default void setCanDrinkPotion(boolean can) {
        core().nbtSet("Paper.CanDrinkPotionDisabled", "bool", !can);
    }

    default boolean canDrinkMilk() {
        return !core().nbtFlag("Paper.CanDrinkMilkDisabled");
    }

    default void setCanDrinkMilk(boolean can) {
        core().nbtSet("Paper.CanDrinkMilkDisabled", "bool", !can);
    }

    default org.bukkit.inventory.Inventory getInventory() {
        return new org.basaltmc.bukkit.inv.EntityContainerInventory(core(), this, 8, "Inventory");
    }
}
