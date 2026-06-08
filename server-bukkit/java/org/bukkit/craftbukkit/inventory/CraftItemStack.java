package org.bukkit.craftbukkit.inventory;

import java.util.Locale;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import org.bukkit.Material;
import org.bukkit.inventory.ItemStack;

/** Converts between the stacks of the internals and the plain Bukkit ones; only the type and count travel. */
public class CraftItemStack extends ItemStack {
    protected CraftItemStack() {
    }

    public static ItemStack asBukkitMirror(net.minecraft.world.item.ItemStack original) {
        return asCraftMirror(original);
    }

    public static ItemStack asCraftMirror(net.minecraft.world.item.ItemStack original) {
        if (original == null || original.isEmpty()) {
            return new ItemStack(Material.AIR, 0);
        }
        Material type = Material.getMaterial(original.getItem().itemKey().getPath().toUpperCase(Locale.ROOT));
        return new ItemStack(type == null ? Material.AIR : type, original.getCount());
    }

    public static net.minecraft.world.item.ItemStack asNMSCopy(ItemStack original) {
        if (original == null || original.getType().isAir()) {
            return net.minecraft.world.item.ItemStack.EMPTY;
        }
        net.minecraft.world.item.Item item = BuiltInRegistries.ITEM.getValue(
            Identifier.parse("minecraft:" + original.getType().name().toLowerCase(Locale.ROOT)));
        return new net.minecraft.world.item.ItemStack(item, original.getAmount());
    }
}
