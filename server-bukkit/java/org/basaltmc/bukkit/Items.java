package org.basaltmc.bukkit;

import java.util.Locale;
import java.util.Map;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Material;
import org.bukkit.inventory.ItemStack;

public final class Items {
    private Items() {}

    /** A stack from the server's item compound: id, count and components; null for nothing. */
    public static ItemStack fromBytes(byte[] bytes) {
        if (bytes == null) {
            return null;
        }
        Map<String, Object> root = Nbt.read(bytes);
        if (root == null) {
            return null;
        }
        Material material = Material.matchMaterial(Nbt.string(root, "id", ""));
        if (material == null || material.isAir()) {
            return null;
        }
        Map<String, Object> components = Nbt.map(root.get("components"));
        return new BasaltItemStack(material, Nbt.integer(root, "count", 1),
            components == null ? Nbt.compound() : components);
    }

    public static byte[] toBytes(ItemStack stack) {
        if (stack == null || stack.getType().isAir() || stack.getAmount() <= 0) {
            return null;
        }
        Map<String, Object> root = Nbt.compound();
        root.put("id", stack.getType().getKey().toString());
        root.put("count", stack.getAmount());
        root.put("components", componentsOf(stack));
        return Nbt.write(root);
    }

    /** The live component map of a stack the server made, or a copy taken through its meta for any other. */
    public static Map<String, Object> componentsOf(ItemStack stack) {
        if (stack instanceof BasaltItemStack own) {
            return own.components();
        }
        org.bukkit.inventory.meta.ItemMeta meta = stack.getItemMeta();
        return meta instanceof org.basaltmc.bukkit.item.MetaItem item ? item.components() : Nbt.compound();
    }

    public static String id(Material material) {
        return material.getKey().toString().toLowerCase(Locale.ROOT);
    }

    /** A stack from the `{id:...,count:...,components:{...}}` text events carry, or null for an empty one. */
    public static ItemStack fromSnbt(String text) {
        if (text == null || text.isEmpty()) {
            return null;
        }
        byte[] wrapped = InventoryNatives.parseSnbt(text);
        Map<String, Object> root = wrapped == null ? null : Nbt.read(wrapped);
        Map<String, Object> value = root == null ? null : Nbt.map(root.get("value"));
        return value == null ? null : org.basaltmc.bukkit.item.Convert.stack(value);
    }

    public static String toSnbt(ItemStack stack) {
        if (stack == null || stack.getType().isAir() || stack.getAmount() <= 0) {
            return "";
        }
        StringBuilder out = new StringBuilder();
        org.basaltmc.bukkit.item.ItemMetas.write(out, org.basaltmc.bukkit.item.Convert.stack(stack));
        return out.toString();
    }
}
