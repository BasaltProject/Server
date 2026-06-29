package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.attribute.Attribute;
import org.bukkit.entity.AbstractHorse;
import org.bukkit.entity.Horse;

public interface AbstractHorseOps extends AbstractHorse, Cored {
    default int getDomestication() {
        return core().nbtInt("Temper");
    }

    default void setDomestication(int level) {
        core().nbtSet("Temper", "int", Math.max(0, Math.min(getMaxDomestication(), level)));
    }

    default int getMaxDomestication() {
        return core().integer("horse_max_temper");
    }

    default double getJumpStrength() {
        return getAttribute(Attribute.JUMP_STRENGTH).getValue();
    }

    default void setJumpStrength(double strength) {
        if (strength < 0.0) {
            throw new IllegalArgumentException("Jump strength cannot be less than zero");
        }
        getAttribute(Attribute.JUMP_STRENGTH).setBaseValue(strength);
    }

    default Horse.Variant getVariant() {
        return switch (core().type.getKey().getKey()) {
            case "donkey" -> Horse.Variant.DONKEY;
            case "mule" -> Horse.Variant.MULE;
            case "skeleton_horse" -> Horse.Variant.SKELETON_HORSE;
            case "zombie_horse" -> Horse.Variant.UNDEAD_HORSE;
            case "llama" -> Horse.Variant.LLAMA;
            default -> Horse.Variant.HORSE;
        };
    }

    default boolean isEatingHaystack() {
        return core().flag("horse_eating");
    }

    default void setEatingHaystack(boolean eating) {
        core().set("horse_eating", eating);
    }

    default org.bukkit.inventory.AbstractHorseInventory getInventory() {
        return new org.basaltmc.bukkit.inv.MountInventory(core(), this);
    }

    default boolean isEatingGrass() {
        return core().flag("horse_eating");
    }

    default void setEatingGrass(boolean eating) {
        core().set("horse_eating", eating);
    }

    default boolean isRearing() {
        return core().flag("horse_rearing");
    }

    default void setRearing(boolean rearing) {
        core().set("horse_rearing", rearing);
    }

    default boolean isEating() {
        return core().flag("horse_mouth_open");
    }

    default void setEating(boolean eating) {
        core().set("horse_mouth_open", eating);
    }

    default void setMaxDomestication(int level) {
        if (level <= 0) {
            throw new IllegalArgumentException("Max domestication (" + level + ") cannot be zero or less");
        }
        core().set("horse_max_temper", level);
    }
}
