package org.bukkit.craftbukkit.inventory;

import org.basaltmc.bukkit.Unsupported;
import org.bukkit.inventory.ItemStack;
public class CraftItemStack extends ItemStack {
    protected CraftItemStack() {
    }
    public static ItemStack asBukkitMirror(net.minecraft.world.item.ItemStack original) {
        throw Unsupported.hit("org.bukkit.craftbukkit.inventory.CraftItemStack.asBukkitMirror");
    }
    public static ItemStack asCraftMirror(net.minecraft.world.item.ItemStack original) {
        throw Unsupported.hit("org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror");
    }

    public static net.minecraft.world.item.ItemStack asNMSCopy(ItemStack original) {
        throw Unsupported.hit("org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy");
    }
}
