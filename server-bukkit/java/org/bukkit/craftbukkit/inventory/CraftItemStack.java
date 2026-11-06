package org.bukkit.craftbukkit.inventory;

import com.mojang.serialization.Dynamic;
import com.mojang.serialization.JavaOps;
import java.util.LinkedHashMap;
import java.util.Locale;
import java.util.Map;
import net.minecraft.core.component.DataComponentPatch;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import org.basaltmc.bukkit.BasaltItemStack;
import org.basaltmc.bukkit.Items;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Material;
import org.bukkit.inventory.ItemStack;

public class CraftItemStack extends BasaltItemStack {
    protected CraftItemStack() {
        super(Material.AIR, 0, Nbt.compound());
    }

    private CraftItemStack(Material type, int amount, Map<String, Object> components) {
        super(type, amount, components);
    }

    public static ItemStack asBukkitMirror(net.minecraft.world.item.ItemStack original) {
        return asCraftMirror(original);
    }

    public static ItemStack asBukkitCopy(net.minecraft.world.item.ItemStack original) {
        if (original == null || original.isEmpty()) {
            return new BasaltItemStack(Material.AIR, 0, Nbt.compound());
        }
        return new BasaltItemStack(materialOf(original), original.getCount(), componentsOf(original));
    }

    public static CraftItemStack asCraftMirror(net.minecraft.world.item.ItemStack original) {
        if (original == null || original.isEmpty()) {
            return new CraftItemStack(Material.AIR, 0, Nbt.compound());
        }
        return new CraftItemStack(materialOf(original), original.getCount(), componentsOf(original));
    }

    public static CraftItemStack asCraftCopy(ItemStack original) {
        if (original == null) {
            return new CraftItemStack(Material.AIR, 0, Nbt.compound());
        }
        return new CraftItemStack(original.getType(), original.getAmount(), Nbt.deepCopy(Items.componentsOf(original)));
    }

    public static CraftItemStack asNewCraftStack(net.minecraft.world.item.Item item) {
        return asNewCraftStack(item, 1);
    }

    public static CraftItemStack asNewCraftStack(net.minecraft.world.item.Item item, int amount) {
        Material type = Material.getMaterial(item.itemKey().getPath().toUpperCase(Locale.ROOT));
        return new CraftItemStack(type == null ? Material.AIR : type, amount, Nbt.compound());
    }

    public static net.minecraft.world.item.ItemStack asNMSCopy(ItemStack original) {
        if (original == null || original.getType().isAir()) {
            return net.minecraft.world.item.ItemStack.EMPTY;
        }
        net.minecraft.world.item.Item item = BuiltInRegistries.ITEM.getValue(
            Identifier.parse("minecraft:" + original.getType().name().toLowerCase(Locale.ROOT)));
        net.minecraft.world.item.ItemStack made = new net.minecraft.world.item.ItemStack(item, original.getAmount());
        Map<String, Object> components = Items.componentsOf(original);
        if (!components.isEmpty()) {
            made.applyComponents(patchOf(components));
        }
        return made;
    }

    private static Material materialOf(net.minecraft.world.item.ItemStack stack) {
        Material type = Material.getMaterial(stack.getItem().itemKey().getPath().toUpperCase(Locale.ROOT));
        return type == null ? Material.AIR : type;
    }

    private static DataComponentPatch patchOf(Map<String, Object> components) {
        Object copy = Nbt.deepCopy(components);
        return DataComponentPatch.CODEC.parse(JavaOps.INSTANCE, copy).result().orElse(DataComponentPatch.EMPTY);
    }

    @SuppressWarnings("unchecked")
    private static Map<String, Object> componentsOf(net.minecraft.world.item.ItemStack stack) {
        Map<String, Object> out = Nbt.compound();
        DataComponentPatch patch = stack.getComponentsPatch();
        if (patch == null || patch == DataComponentPatch.EMPTY) {
            return out;
        }
        Object encoded = DataComponentPatch.CODEC.encodeStart(JavaOps.INSTANCE, patch).result().orElse(null);
        if (encoded instanceof Dynamic<?> dynamic) {
            encoded = dynamic.getValue();
        }
        if (encoded instanceof Map<?, ?> map) {
            for (Map.Entry<?, ?> entry : map.entrySet()) {
                String key = String.valueOf(entry.getKey());
                if (!key.startsWith("!")) {
                    out.put(key, Nbt.copy(entry.getValue()));
                }
            }
        }
        return out;
    }
}
