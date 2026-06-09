package org.bukkit.craftbukkit.inventory;

import java.util.Locale;
import net.minecraft.core.component.DataComponentPatch;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import org.bukkit.Material;
import org.bukkit.inventory.ItemStack;

public class CraftItemStack extends ItemStack {
    private DataComponentPatch components = DataComponentPatch.EMPTY;

    protected CraftItemStack() {
    }

    private CraftItemStack(Material type, int amount, DataComponentPatch components) {
        super(type, amount);
        this.components = components;
    }

    public static ItemStack asBukkitMirror(net.minecraft.world.item.ItemStack original) {
        return asCraftMirror(original);
    }

    public static ItemStack asCraftMirror(net.minecraft.world.item.ItemStack original) {
        if (original == null || original.isEmpty()) {
            return new ItemStack(Material.AIR, 0);
        }
        Material type = Material.getMaterial(original.getItem().itemKey().getPath().toUpperCase(Locale.ROOT));
        return new CraftItemStack(type == null ? Material.AIR : type, original.getCount(), original.getComponentsPatch());
    }

    public static net.minecraft.world.item.ItemStack asNMSCopy(ItemStack original) {
        if (original == null || original.getType().isAir()) {
            return net.minecraft.world.item.ItemStack.EMPTY;
        }
        net.minecraft.world.item.Item item = BuiltInRegistries.ITEM.getValue(
            Identifier.parse("minecraft:" + original.getType().name().toLowerCase(Locale.ROOT)));
        net.minecraft.world.item.ItemStack made = new net.minecraft.world.item.ItemStack(item, original.getAmount());
        if (original instanceof CraftItemStack craft && !craft.components.isEmpty()) {
            made.applyComponents(craft.components);
        }
        return made;
    }
}
