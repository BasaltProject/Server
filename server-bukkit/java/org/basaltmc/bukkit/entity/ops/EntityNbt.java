package org.basaltmc.bukkit.entity.ops;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.basaltmc.bukkit.entity.EntityCore;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.World;
import org.bukkit.block.data.BlockData;

/** Reads and writes the saved data of an entity as plain values, for the kinds of data one plugin call touches. */
final class EntityNbt {
    private EntityNbt() {}

    static Map<String, Object> compound(EntityCore core, String path) {
        byte[] stored = core.nbt(path);
        Map<String, Object> read = stored == null ? null : Nbt.readNamed(stored);
        return read == null ? Nbt.compound() : read;
    }

    static void put(EntityCore core, String key, Object value) {
        Map<String, Object> patch = Nbt.compound();
        patch.put(key, value);
        core.mergeNbt(Nbt.writeNamed(patch));
    }

    static float real(Object value, double fallback) {
        return value instanceof Number number ? number.floatValue() : (float) fallback;
    }

    static List<Object> floats(float... values) {
        List<Object> out = new ArrayList<>();
        for (float value : values) {
            out.add(value);
        }
        return out;
    }

    static List<Object> ints(int... values) {
        List<Object> out = new ArrayList<>();
        for (int value : values) {
            out.add(value);
        }
        return out;
    }

    static float at(List<Object> list, int index, double fallback) {
        return list != null && index < list.size() && list.get(index) instanceof Number number ? number.floatValue()
            : (float) fallback;
    }

    static Map<String, Object> blockState(BlockData data) {
        String text = data.getAsString();
        int open = text.indexOf('[');
        Map<String, Object> state = Nbt.compound();
        state.put("Name", open < 0 ? text : text.substring(0, open));
        if (open >= 0) {
            Map<String, Object> properties = Nbt.compound();
            for (String pair : text.substring(open + 1, text.length() - 1).split(",")) {
                String[] parts = pair.split("=", 2);
                properties.put(parts[0], parts[1]);
            }
            state.put("Properties", properties);
        }
        return state;
    }

    static BlockData blockData(Map<String, Object> state) {
        if (state == null || !state.containsKey("Name")) {
            return null;
        }
        StringBuilder text = new StringBuilder(Nbt.string(state, "Name", "minecraft:air"));
        Map<String, Object> properties = Nbt.map(state.get("Properties"));
        if (properties != null && !properties.isEmpty()) {
            text.append('[');
            boolean first = true;
            for (Map.Entry<String, Object> property : properties.entrySet()) {
                text.append(first ? "" : ",").append(property.getKey()).append('=').append(property.getValue());
                first = false;
            }
            text.append(']');
        }
        return Bukkit.createBlockData(text.toString());
    }

    static Location position(World world, Object stored) {
        if (stored instanceof int[] at && at.length == 3) {
            return new Location(world, at[0], at[1], at[2]);
        }
        return null;
    }

    static int[] position(Location location) {
        return new int[] {location.getBlockX(), location.getBlockY(), location.getBlockZ()};
    }

    static org.bukkit.inventory.ItemStack item(EntityCore core, String key, org.bukkit.Material fallback) {
        Map<String, Object> stored = Nbt.map(compound(core, "").get(key));
        org.bukkit.inventory.ItemStack stack = stored == null ? null
            : org.basaltmc.bukkit.item.Convert.stack(stored);
        return stack != null ? stack : fallback == null ? null : new org.bukkit.inventory.ItemStack(fallback);
    }

    static void storeItem(EntityCore core, String key, org.bukkit.inventory.ItemStack stack) {
        if (stack == null || stack.getType().isAir()) {
            core.nbtRemove(key);
        } else {
            put(core, key, org.basaltmc.bukkit.item.Convert.stack(stack));
        }
    }

    @SuppressWarnings("unchecked")
    static <T extends org.bukkit.Keyed> T variant(EntityCore core, String key, io.papermc.paper.registry.RegistryKey<T> registry,
            String fallback) {
        Object id = compound(core, "").get(key);
        org.bukkit.Registry<T> held = io.papermc.paper.registry.RegistryAccess.registryAccess().getRegistry(registry);
        T found = id == null ? null : held.get(org.bukkit.NamespacedKey.fromString(String.valueOf(id)));
        return found != null ? found : held.get(org.bukkit.NamespacedKey.minecraft(fallback));
    }

    static void variant(EntityCore core, String key, org.bukkit.Keyed value) {
        core.nbtSet(key, "string", value.getKey().toString());
    }
}
