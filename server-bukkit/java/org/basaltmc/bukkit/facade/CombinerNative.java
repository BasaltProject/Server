package org.basaltmc.bukkit.facade;

import java.util.Map;
import com.mojang.serialization.JavaOps;
import net.minecraft.core.component.DataComponentPatch;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import org.basaltmc.bukkit.InventoryNatives;
import org.basaltmc.bukkit.item.Nbt;

/** Anvil and smithing results computed by the server's recipe and repair services. */
public final class CombinerNative {
    private CombinerNative() {}

    public record Anvil(ItemStack result, int cost, int materials) {}

    private static byte[] stack(ItemStack stack) {
        Map<String, Object> out = Nbt.compound();
        if (stack != null && !stack.isEmpty()) {
            out.put("id", stack.getItem().itemKey().toString());
            out.put("count", stack.getCount());
            Object raw = DataComponentPatch.CODEC.encodeStart(JavaOps.INSTANCE, stack.getComponentsPatch()).result().orElse(null);
            if (raw instanceof Map<?, ?> components && !components.isEmpty()) {
                Map<String, Object> copy = Nbt.compound();
                components.forEach((key, value) -> copy.put(String.valueOf(key), value));
                out.put("components", copy);
            }
        }
        return Nbt.write(out);
    }

    private static ItemStack read(Object raw) {
        Map<String, Object> map = Nbt.map(raw);
        if (map == null || map.isEmpty()) {
            return ItemStack.EMPTY;
        }
        Item item = BuiltInRegistries.ITEM.getValue(Identifier.parse(Nbt.string(map, "id", "minecraft:air")));
        ItemStack made = new ItemStack(item, Nbt.integer(map, "count", 1));
        Map<String, Object> components = Nbt.map(map, "components");
        if (components != null && !components.isEmpty()) {
            made.applyComponents(DataComponentPatch.CODEC.parse(JavaOps.INSTANCE, components).result().orElse(DataComponentPatch.EMPTY));
        }
        return made;
    }

    public static Anvil anvil(ItemStack left, ItemStack right, String name) {
        byte[] bytes = InventoryNatives.anvil(stack(left), stack(right), name == null ? "" : name);
        Map<String, Object> made = bytes == null ? null : Nbt.read(bytes);
        if (made == null) {
            return new Anvil(ItemStack.EMPTY, 0, 0);
        }
        return new Anvil(read(made.get("result")), Nbt.integer(made, "cost", 0), Nbt.integer(made, "materials", 0));
    }

    public static ItemStack smithing(ItemStack template, ItemStack base, ItemStack addition) {
        byte[] bytes = InventoryNatives.smithing(stack(template), stack(base), stack(addition));
        Map<String, Object> made = bytes == null ? null : Nbt.read(bytes);
        return made == null ? ItemStack.EMPTY : read(made.get("result"));
    }
}
