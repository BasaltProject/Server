// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.block.Block;

public record BlockItemTagId(TagKey<Block> block, TagKey<Item> item) {
    public static BlockItemTagId create(Identifier blockId, Identifier itemId) {
        return new BlockItemTagId(TagKey.create((net.minecraft.resources.ResourceKey) Registries.BLOCK, blockId), TagKey.create((net.minecraft.resources.ResourceKey) Registries.ITEM, itemId));
    }

    public static BlockItemTagId create(String blockName, String itemName) {
        return create(Identifier.withDefaultNamespace(blockName), Identifier.withDefaultNamespace(itemName));
    }

    public static BlockItemTagId create(String name) {
        return create(name, name);
    }
}
