package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Piglin;

public interface PiglinOps extends Piglin, Cored {
    default boolean isAbleToHunt() {
        return !core().flag("piglin_cannot_hunt");
    }

    default void setIsAbleToHunt(boolean able) {
        core().set("piglin_cannot_hunt", !able);
    }

    default boolean isChargingCrossbow() {
        return core().flag("piglin_charging");
    }

    default void setChargingCrossbow(boolean charging) {
        core().set("piglin_charging", charging);
    }

    default boolean isDancing() {
        return core().flag("piglin_dancing");
    }

    default void setDancing(boolean dancing) {
        core().call("piglin_dance", dancing, -1L);
    }

    default void setDancing(long duration) {
        core().call("piglin_dance", true, duration);
    }

    private java.util.Set<org.bukkit.Material> items(String kind) {
        java.util.Set<org.bukkit.Material> out = new java.util.HashSet<>();
        if (core().call("piglin_items", kind) instanceof String[] names) {
            for (String name : names) {
                org.bukkit.Material material = org.bukkit.Material.matchMaterial(name);
                if (material != null) {
                    out.add(material);
                }
            }
        }
        return java.util.Collections.unmodifiableSet(out);
    }

    private boolean edit(String kind, org.bukkit.Material material, boolean add) {
        return material.isItem() && core().test("piglin_item_edit", kind, material.getKey().toString(), add);
    }

    default boolean addBarterMaterial(org.bukkit.Material material) {
        return edit("barter", material, true);
    }

    default boolean removeBarterMaterial(org.bukkit.Material material) {
        return edit("barter", material, false);
    }

    default boolean addMaterialOfInterest(org.bukkit.Material material) {
        return edit("interest", material, true);
    }

    default boolean removeMaterialOfInterest(org.bukkit.Material material) {
        return edit("interest", material, false);
    }

    default java.util.Set<org.bukkit.Material> getBarterList() {
        return items("barter");
    }

    default java.util.Set<org.bukkit.Material> getInterestList() {
        return items("interest");
    }

    default org.bukkit.inventory.Inventory getInventory() {
        return new org.basaltmc.bukkit.inv.HeldInventory(core(), this, "piglin", 8);
    }
}
