package org.basaltmc.bukkit.inv;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.basaltmc.bukkit.BasaltItemStack;
import org.basaltmc.bukkit.WorldNatives;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.World;
import org.bukkit.entity.HumanEntity;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.InventoryHolder;
import org.bukkit.inventory.ItemStack;

/** The items of a container block, kept in its block entity, so a window and a plugin always see the same stacks. */
public class BlockInventory extends AbstractInventory {
    protected final String world;
    protected final int x;
    protected final int y;
    protected final int z;
    protected final InventoryType type;
    protected final int size;
    protected final InventoryHolder holder;

    public BlockInventory(String world, int x, int y, int z, InventoryType type, int size, InventoryHolder holder) {
        this.world = world;
        this.x = x;
        this.y = y;
        this.z = z;
        this.type = type;
        this.size = size;
        this.holder = holder;
    }

    public String worldName() {
        return world;
    }

    public int blockX() {
        return x;
    }

    public int blockY() {
        return y;
    }

    public int blockZ() {
        return z;
    }

    public static int sizeOf(String block) {
        switch (block) {
            case "hopper", "brewing_stand":
                return 5;
            case "dispenser", "dropper", "crafter":
                return 9;
            case "furnace", "blast_furnace", "smoker":
                return 3;
            case "lectern", "beacon", "jukebox", "decorated_pot":
                return 1;
            case "chiseled_bookshelf":
                return 6;
            default:
                return 27;
        }
    }

    protected Map<String, Object> entity() {
        return Nbt.readNamed(WorldNatives.blockEntityNbt(world, x, y, z));
    }

    @Override
    public int getSize() {
        return size;
    }

    @Override
    public ItemStack getItem(int slot) {
        if (slot < 0 || slot >= size) {
            throw new IndexOutOfBoundsException("Slot " + slot + " is outside of " + size);
        }
        Map<String, Object> data = entity();
        List<Object> items = data == null ? null : Nbt.list(data.get("Items"));
        if (items != null) {
            for (Object entry : items) {
                Map<String, Object> item = Nbt.map(entry);
                if (item != null && Nbt.integer(item, "Slot", -1) == slot) {
                    ItemStack stack = Convert.stack(item);
                    return stack instanceof BasaltItemStack own ? own.bound(stored -> setItem(slot, stored)) : stack;
                }
            }
        }
        return null;
    }

    @Override
    public void setItem(int slot, ItemStack item) {
        if (slot < 0 || slot >= size) {
            throw new IndexOutOfBoundsException("Slot " + slot + " is outside of " + size);
        }
        Map<String, Object> data = entity();
        if (data == null) {
            return;
        }
        List<Object> items = Nbt.list(data.get("Items"));
        List<Object> kept = new ArrayList<>();
        if (items != null) {
            for (Object entry : items) {
                Map<String, Object> existing = Nbt.map(entry);
                if (existing != null && Nbt.integer(existing, "Slot", -1) != slot) {
                    kept.add(existing);
                }
            }
        }
        if (item != null && !item.getType().isAir() && item.getAmount() > 0) {
            Map<String, Object> stored = Convert.stack(item);
            stored.put("Slot", (byte) slot);
            kept.add(stored);
        }
        data.put("Items", kept);
        WorldNatives.setBlockEntityNbt(world, x, y, z, Nbt.writeNamed(data));
    }

    @Override
    public List<HumanEntity> getViewers() {
        List<HumanEntity> out = new ArrayList<>();
        for (org.bukkit.entity.Player player : Bukkit.getOnlinePlayers()) {
            if (TypedInventories.unwrap(player.getOpenInventory().getTopInventory()) instanceof BlockInventory other
                && other.sameBlock(this)) {
                out.add(player);
            }
        }
        return out;
    }

    boolean sameBlock(BlockInventory other) {
        return other.world.equals(world) && other.x == x && other.y == y && other.z == z;
    }

    @Override
    public InventoryType getType() {
        return type;
    }

    @Override
    public InventoryHolder getHolder() {
        return holder;
    }

    @Override
    public InventoryHolder getHolder(boolean useSnapshot) {
        return holder;
    }

    @Override
    public Location getLocation() {
        World found = Bukkit.getWorld(world);
        return found == null ? null : new Location(found, x, y, z);
    }
}
