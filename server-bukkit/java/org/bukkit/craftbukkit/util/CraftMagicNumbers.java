package org.bukkit.craftbukkit.util;

import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.block.Block;
import org.bukkit.Material;

public final class CraftMagicNumbers {
    private CraftMagicNumbers() {}

    public static Block getBlock(Material material) {
        Block block = BuiltInRegistries.BLOCK.getValue(CraftNamespacedKey.toMinecraft(material.getKey()));
        return material.isBlock() ? block : null;
    }

    public static Item getItem(Material material) {
        return material.isItem()
            ? BuiltInRegistries.ITEM.getValue(CraftNamespacedKey.toMinecraft(material.getKey())) : null;
    }

    public static Material getMaterial(Block block) {
        return block == null ? Material.AIR : materialOf(block.blockKey());
    }

    public static Material getMaterial(Item item) {
        return item == null ? Material.AIR : materialOf(item.itemKey());
    }

    private static Material materialOf(Identifier key) {
        Material found = Material.getMaterial(key.getPath().toUpperCase(java.util.Locale.ROOT));
        return found == null ? Material.AIR : found;
    }
}
