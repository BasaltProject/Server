package org.basaltmc.bukkit;

import java.util.Locale;
import org.bukkit.Material;
import org.bukkit.inventory.ItemStack;

final class Items {
    private Items() {}

    static ItemStack fromStrings(String[] flat) {
        if (flat == null) {
            return null;
        }
        Material material = Material.matchMaterial(flat[0]);
        if (material == null || material.isAir()) {
            return null;
        }
        return new ItemStack(material, Integer.parseInt(flat[1]));
    }

    static String[] toStrings(ItemStack stack) {
        if (stack == null || stack.getType().isAir() || stack.getAmount() <= 0) {
            return null;
        }
        return new String[] {stack.getType().getKey().toString(), Integer.toString(stack.getAmount()), "0", "", "0", ""};
    }

    static String id(Material material) {
        return material.getKey().toString().toLowerCase(Locale.ROOT);
    }
}
