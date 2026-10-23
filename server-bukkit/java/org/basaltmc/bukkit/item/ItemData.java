package org.basaltmc.bukkit.item;

import java.util.HashMap;
import java.util.Map;
import org.basaltmc.bukkit.InventoryNatives;

/** What the server's registries know about one item kind; read once per id. */
public final class ItemData {
    private static final Map<String, ItemData> cache = new HashMap<>();
    private static final ItemData kUnknown = new ItemData(Nbt.compound());

    private final Map<String, Object> data;

    private ItemData(Map<String, Object> data) {
        this.data = data;
    }

    public static synchronized ItemData of(String id) {
        ItemData found = cache.get(id);
        if (found == null) {
            byte[] bytes = InventoryNatives.itemData(id);
            Map<String, Object> read = bytes == null ? null : Nbt.read(bytes);
            found = read == null ? kUnknown : new ItemData(read);
            cache.put(id, found);
        }
        return found;
    }

    public int maxStackSize() {
        return Nbt.integer(data, "max_stack_size", 64);
    }

    public int maxDamage() {
        return Nbt.integer(data, "max_damage", 0);
    }

    public int burnTicks() {
        return Nbt.integer(data, "burn_ticks", 0);
    }

    public int useCooldownTicks() {
        return Nbt.integer(data, "use_cooldown_ticks", 0);
    }

    public Map<String, Object> food() {
        return Nbt.map(data.get("food"));
    }

    public Map<String, Object> armor() {
        return Nbt.map(data.get("armor"));
    }

    public Map<String, Object> weapon() {
        return Nbt.map(data.get("weapon"));
    }

    public String craftingRemainder() {
        return Nbt.string(data, "crafting_remainder", null);
    }

    public String block() {
        return Nbt.string(data, "block", null);
    }
}
