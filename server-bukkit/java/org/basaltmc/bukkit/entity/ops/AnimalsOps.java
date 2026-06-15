package org.basaltmc.bukkit.entity.ops;

import java.util.UUID;
import org.bukkit.entity.Animals;

public interface AnimalsOps extends Animals, org.basaltmc.bukkit.entity.Cored {
    default int getLoveModeTicks() {
        return core().integer("love_ticks");
    }

    default void setLoveModeTicks(int ticks) {
        core().set("love_ticks", Math.max(0, ticks));
    }

    default boolean isLoveMode() {
        return getLoveModeTicks() > 0;
    }

    default UUID getBreedCause() {
        String cause = core().text("love_cause");
        return cause == null ? null : UUID.fromString(cause);
    }

    default void setBreedCause(UUID uuid) {
        core().set("love_cause", uuid == null ? null : uuid.toString());
    }

    default boolean isBreedItem(org.bukkit.Material material) {
        return core().test("is_breed_item", material.getKey().toString());
    }

    default boolean isBreedItem(org.bukkit.inventory.ItemStack stack) {
        return isBreedItem(stack.getType());
    }
}
