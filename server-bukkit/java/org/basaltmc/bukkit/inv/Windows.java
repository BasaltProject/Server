package org.basaltmc.bukkit.inv;

import java.util.HashMap;
import java.util.Map;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.InventoryNatives;
import org.basaltmc.bukkit.Items;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.item.Texts;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.entity.HumanEntity;
import org.bukkit.event.inventory.InventoryCloseEvent;
import org.bukkit.event.inventory.InventoryOpenEvent;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.Inventory;
import org.bukkit.inventory.InventoryView;
import org.bukkit.inventory.ItemStack;

/** The window each player has open, with the Java object that stands for it. */
public final class Windows {
    private static final Map<String, BasaltInventoryView> views = new HashMap<>();

    private Windows() {}

    private static String uuid(HumanEntity player) {
        return player.getUniqueId().toString();
    }

    /** The view the player has open now, made from the server's window when a plugin did not open it. */
    public static synchronized BasaltInventoryView current(HumanEntity player) {
        String id = uuid(player);
        String[] info = InventoryNatives.windowInfo(id);
        if (info == null) {
            return null;
        }
        int container = Integer.parseInt(info[0]);
        BasaltInventoryView cached = views.get(id);
        if (cached != null && cached.containerId() == container) {
            return cached;
        }
        String kind = info[1];
        int slots = Integer.parseInt(info[2]);
        InventoryType type = InventoryTypes.typeOf(kind);
        BasaltInventoryView view;
        if (container == 0) {
            view = new BasaltInventoryView(player,
                TypedInventories.wrap(new WindowInventory(id, InventoryType.CRAFTING, 5, null)),
                InventoryType.CRAFTING.defaultTitle(), true, 0);
        } else {
            int size = Math.max(0, slots - 36);
            Inventory top;
            if (info.length >= 8) {
                String world = info[4];
                int x = Integer.parseInt(info[5]);
                int y = Integer.parseInt(info[6]);
                int z = Integer.parseInt(info[7]);
                top = new BlockInventory(world, x, y, z, type, size, null);
            } else {
                top = new WindowInventory(id, type, size, null);
            }
            view = new BasaltInventoryView(player, TypedInventories.wrap(top), type.defaultTitle(), false, container);
        }
        views.put(id, view);
        return view;
    }

    /** A view of a window that is about to open, for the event that asks whether it may. */
    public static BasaltInventoryView pending(HumanEntity player, String kind, String world, int x, int y, int z) {
        InventoryType type = InventoryTypes.typeOf(kind);
        Inventory top;
        switch (kind) {
            case "chest", "trapped_chest", "barrel", "shulker_box", "hopper", "dispenser", "dropper", "furnace",
                "blast_furnace", "smoker", "brewing_stand", "crafter", "lectern":
                top = new BlockInventory(world, x, y, z, type, BlockInventory.sizeOf(kind), null);
                break;
            default:
                top = new WindowInventory(uuid(player), type, Math.max(type.getDefaultSize(), 0), null);
                break;
        }
        return new BasaltInventoryView(player, TypedInventories.wrap(top), type.defaultTitle(), false, -1);
    }

    public static synchronized BasaltInventoryView cached(HumanEntity player) {
        return views.get(uuid(player));
    }

    public static synchronized void closed(HumanEntity player) {
        BasaltInventoryView view = views.remove(uuid(player));
        if (view != null && TypedInventories.unwrap(view.getTopInventory()) instanceof CustomInventory custom) {
            custom.detach(player);
        }
    }

    static byte[] titleOf(Component title) {
        Map<String, Object> wrapper = Nbt.compound();
        wrapper.put("value", Texts.toNbt(title));
        return Nbt.write(wrapper);
    }

    public static InventoryView open(HumanEntity player, Inventory inventory) {
        Inventory real = TypedInventories.unwrap(inventory);
        if (real instanceof BlockInventory block) {
            if (!block.worldName().equals(player.getWorld().getName())) {
                return null;
            }
            int container = InventoryNatives.windowOpenBlock(uuid(player), block.blockX(), block.blockY(), block.blockZ());
            return container == 0 ? null : current(player);
        }
        if (real instanceof EntityContainerInventory entity) {
            int container = InventoryNatives.windowOpenEntity(uuid(player), entity.entityId());
            return container == 0 ? null : current(player);
        }
        if (!(real instanceof CustomInventory custom)) {
            throw org.basaltmc.bukkit.Unsupported.hit("HumanEntity#openInventory of " + real.getClass().getSimpleName());
        }
        String kind = InventoryTypes.kindOf(custom.getType());
        if (kind == null) {
            throw new IllegalArgumentException("Cannot open an inventory of type " + custom.getType());
        }
        BasaltInventoryView view = new BasaltInventoryView(player, inventory, custom.title(), false, -1);
        return open(player, view);
    }

    public static InventoryView open(HumanEntity player, BasaltInventoryView view) {
        Inventory top = TypedInventories.unwrap(view.getTopInventory());
        if (top instanceof BlockInventory || top instanceof EntityContainerInventory) {
            return open(player, top);
        }
        if (!(top instanceof CustomInventory custom)) {
            throw org.basaltmc.bukkit.Unsupported.hit("HumanEntity#openInventory of " + top.getClass().getSimpleName());
        }
        InventoryOpenEvent event = new InventoryOpenEvent(view);
        Bukkit.getPluginManager().callEvent(event);
        if (event.isCancelled()) {
            return null;
        }
        String id = uuid(player);
        ItemStack[] contents = new ItemStack[custom.getSize()];
        byte[][] encoded = new byte[contents.length][];
        for (int slot = 0; slot < contents.length; slot++) {
            encoded[slot] = Items.toBytes(custom.raw(slot));
        }
        int container = InventoryNatives.windowOpen(id, InventoryTypes.kindOf(custom.getType()), custom.getSize(),
            titleOf(custom.title()), encoded, true);
        if (container == 0) {
            return null;
        }
        BasaltInventoryView opened = new BasaltInventoryView(player, view.getTopInventory(), custom.title(), false,
            container);
        synchronized (Windows.class) {
            views.put(id, opened);
        }
        custom.attach(player);
        return opened;
    }

    public static void closeCurrent(HumanEntity player) {
        InventoryNatives.windowClose(uuid(player));
    }

    /** After the window changed behind the plugin's back, refreshes its inventory and every other viewer. */
    public static void synchronize(HumanEntity player) {
        BasaltInventoryView view = cached(player);
        if (view == null || !(TypedInventories.unwrap(view.getTopInventory()) instanceof CustomInventory custom)) {
            return;
        }
        String id = uuid(player);
        for (int slot = 0; slot < custom.getSize(); slot++) {
            ItemStack now = Items.fromBytes(InventoryNatives.windowItem(id, slot));
            ItemStack was = custom.raw(slot);
            boolean same = now == null ? was == null : was != null && now.equals(was);
            if (same) {
                continue;
            }
            custom.absorb(slot, now);
            byte[] encoded = Items.toBytes(now);
            for (HumanEntity other : custom.getViewers()) {
                if (!other.equals(player)) {
                    InventoryNatives.setWindowItem(uuid(other), slot, encoded);
                }
            }
        }
    }

    public static InventoryCloseEvent.Reason reasonOf(String name) {
        switch (name) {
            case "plugin":
                return InventoryCloseEvent.Reason.PLUGIN;
            case "player":
                return InventoryCloseEvent.Reason.PLAYER;
            case "disconnect":
                return InventoryCloseEvent.Reason.DISCONNECT;
            case "death":
                return InventoryCloseEvent.Reason.DEATH;
            case "teleport":
                return InventoryCloseEvent.Reason.TELEPORT;
            case "open_new":
                return InventoryCloseEvent.Reason.OPEN_NEW;
            case "cant_use":
                return InventoryCloseEvent.Reason.CANT_USE;
            default:
                return InventoryCloseEvent.Reason.UNKNOWN;
        }
    }
}
