package org.basaltmc.bukkit.tile;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.EnumMap;
import java.util.List;
import java.util.Map;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Material;
import org.bukkit.block.BlockState;
import org.bukkit.block.ChiseledBookshelf;
import org.bukkit.block.DecoratedPot;
import org.bukkit.block.Jukebox;
import org.bukkit.block.Lectern;
import org.bukkit.block.Shelf;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.ChiseledBookshelfInventory;
import org.bukkit.inventory.DecoratedPotInventory;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.JukeboxInventory;
import org.bukkit.inventory.LecternInventory;
import org.bukkit.inventory.ShelfInventory;
import org.bukkit.util.Vector;

/** The blocks that hold a few items in a fixed place: a record, a book, a bookshelf, a pot, a shelf. */
final class StorageKinds {
    private StorageKinds() {}

    private static Map<String, Object> single(Tile tile, String key) {
        return tile.map(key);
    }

    static final class JukeboxKind implements TileKind {
        @Override
        public Class<? extends BlockState> face() {
            return Jukebox.class;
        }

        private static ItemStack record(Tile tile) {
            Map<String, Object> stack = single(tile, "RecordItem");
            return stack == null ? null : Convert.stack(stack);
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            switch (name) {
                case "getInventory", "getSnapshotInventory":
                    return Inventories.of(tile, InventoryType.JUKEBOX, 1, JukeboxInventory.class);
                case "hasRecord":
                    return record(tile) != null;
                case "getRecord": {
                    ItemStack found = record(tile);
                    return found == null ? new ItemStack(Material.AIR) : found;
                }
                case "setRecord": {
                    ItemStack stack = (ItemStack) args[0];
                    if (stack == null || stack.getType().isAir()) {
                        tile.put("RecordItem", null);
                        tile.put("ticks_since_song_started", null);
                    } else {
                        tile.put("RecordItem", Convert.stack(stack));
                    }
                    return null;
                }
                case "getPlaying": {
                    ItemStack found = record(tile);
                    return found == null ? Material.AIR : found.getType();
                }
                case "setPlaying": {
                    Material material = (Material) args[0];
                    if (material == null || material.isAir()) {
                        tile.put("RecordItem", null);
                    } else {
                        tile.put("RecordItem", Convert.stack(new ItemStack(material)));
                    }
                    return null;
                }
                case "isPlaying":
                    return record(tile) != null && tile.data.get("ticks_since_song_started") != null;
                case "startPlaying":
                    if (record(tile) == null) {
                        return false;
                    }
                    tile.put("ticks_since_song_started", 0L);
                    return true;
                case "stopPlaying":
                    tile.put("ticks_since_song_started", null);
                    return null;
                case "eject": {
                    ItemStack found = record(tile);
                    if (found == null) {
                        return false;
                    }
                    tile.put("RecordItem", null);
                    tile.put("ticks_since_song_started", null);
                    tile.world.dropItemNaturally(tile.location().add(0.5, 1.0, 0.5), found);
                    return true;
                }
                default:
                    return Tile.kNone;
            }
        }
    }

    static final class LecternKind implements TileKind {
        @Override
        public Class<? extends BlockState> face() {
            return Lectern.class;
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            switch (name) {
                case "getInventory", "getSnapshotInventory":
                    return Inventories.of(tile, InventoryType.LECTERN, 1, LecternInventory.class);
                case "getPage":
                    return tile.integer("Page", 0);
                case "setPage":
                    tile.put("Page", (Integer) args[0]);
                    return null;
                default:
                    return Tile.kNone;
            }
        }
    }

    static final class ChiseledBookshelfKind implements TileKind {
        @Override
        public Class<? extends BlockState> face() {
            return ChiseledBookshelf.class;
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            switch (name) {
                case "getInventory", "getSnapshotInventory":
                    return Inventories.of(tile, InventoryType.CHISELED_BOOKSHELF, 6, ChiseledBookshelfInventory.class);
                case "getLastInteractedSlot":
                    return tile.integer("last_interacted_slot", -1);
                case "setLastInteractedSlot":
                    tile.put("last_interacted_slot", (Integer) args[0]);
                    return null;
                case "getSlot": {
                    Vector hit = (Vector) args[0];
                    String facing = tile.property("facing");
                    if (facing == null) {
                        return -1;
                    }
                    double horizontal = switch (facing) {
                        case "north" -> 1.0 - hit.getX();
                        case "south" -> hit.getX();
                        case "west" -> hit.getZ();
                        default -> 1.0 - hit.getZ();
                    };
                    int column = horizontal < 0.375 ? 0 : horizontal < 0.6875 ? 1 : 2;
                    int row = hit.getY() >= 0.5 ? 0 : 1;
                    return column + row * 3;
                }
                default:
                    return Tile.kNone;
            }
        }
    }

    static final class ShelfKind implements TileKind {
        @Override
        public Class<? extends BlockState> face() {
            return Shelf.class;
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            if (name.equals("getInventory") || name.equals("getSnapshotInventory")) {
                return Inventories.of(tile, InventoryType.SHELF, 3, ShelfInventory.class);
            }
            return Tile.kNone;
        }
    }

    static final class PotKind implements TileKind {
        private static final DecoratedPot.Side[] kSides = {DecoratedPot.Side.BACK, DecoratedPot.Side.LEFT,
            DecoratedPot.Side.RIGHT, DecoratedPot.Side.FRONT};

        @Override
        public Class<? extends BlockState> face() {
            return DecoratedPot.class;
        }

        private static List<Object> sherds(Tile tile) {
            List<Object> list = Nbt.list(tile.data, "sherds");
            List<Object> out = new ArrayList<>();
            for (int i = 0; i < 4; i++) {
                out.add(list != null && i < list.size() ? list.get(i) : "minecraft:brick");
            }
            return out;
        }

        private static Material material(Object id) {
            Material material = Material.matchMaterial(String.valueOf(id));
            return material == null ? Material.BRICK : material;
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            switch (name) {
                case "getInventory", "getSnapshotInventory":
                    return Inventories.of(tile, InventoryType.DECORATED_POT, 1, DecoratedPotInventory.class);
                case "getSherd":
                    return material(sherds(tile).get(((DecoratedPot.Side) args[0]).ordinal()));
                case "setSherd": {
                    List<Object> list = sherds(tile);
                    Material material = (Material) args[1];
                    list.set(((DecoratedPot.Side) args[0]).ordinal(),
                        (material == null ? Material.BRICK : material).getKey().toString());
                    tile.put("sherds", list);
                    return null;
                }
                case "getSherds": {
                    Map<DecoratedPot.Side, Material> out = new EnumMap<>(DecoratedPot.Side.class);
                    List<Object> list = sherds(tile);
                    for (DecoratedPot.Side side : kSides) {
                        out.put(side, material(list.get(side.ordinal())));
                    }
                    return out;
                }
                case "getShards": {
                    List<Material> out = new ArrayList<>();
                    for (Object id : sherds(tile)) {
                        out.add(material(id));
                    }
                    return out;
                }
                default:
                    return Tile.kNone;
            }
        }
    }
}
