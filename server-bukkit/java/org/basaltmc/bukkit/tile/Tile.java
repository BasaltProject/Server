package org.basaltmc.bukkit.tile;

import java.util.Map;
import org.basaltmc.bukkit.Blocks;
import org.basaltmc.bukkit.WorldNatives;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.World;
import org.bukkit.block.Block;
import org.bukkit.block.BlockState;
import org.bukkit.block.data.BlockData;

public final class Tile {
    public static final Object kNone = new Object();

    public final World world;
    public final int x;
    public final int y;
    public final int z;
    public final String block;
    public final BlockState base;
    public final Map<String, Object> data;
    public BlockState self;

    Tile(BlockState base, Map<String, Object> data) {
        this.base = base;
        this.world = base.getWorld();
        this.x = base.getX();
        this.y = base.getY();
        this.z = base.getZ();
        this.block = base.getType().getKey().getKey();
        this.data = data;
    }

    public Material material() {
        return base.getType();
    }

    public BlockData blockData() {
        return base.getBlockData();
    }

    /** A property of the block state as text, or null when the block has none of that name. */
    public String property(String name) {
        String text = base.getBlockData().getAsString();
        int open = text.indexOf('[');
        if (open < 0) {
            return null;
        }
        for (String part : text.substring(open + 1, text.length() - 1).split(",")) {
            int at = part.indexOf('=');
            if (at > 0 && part.substring(0, at).equals(name)) {
                return part.substring(at + 1);
            }
        }
        return null;
    }

    public Block live() {
        return world.getBlockAt(x, y, z);
    }

    public Location location() {
        return new Location(world, x, y, z);
    }

    /** The current block entity data of the placed block, or an empty compound when it has none. */
    public static Map<String, Object> read(String world, int x, int y, int z) {
        Blocks.flushAll();
        Map<String, Object> found = Nbt.readNamed(WorldNatives.blockEntityNbt(world, x, y, z));
        return found == null ? Nbt.compound() : found;
    }

    /** Puts the data on the block entity, giving the block one when it has none. */
    void write() {
        Blocks.flushAll();
        byte[] bytes = Nbt.writeNamed(data);
        if (WorldNatives.blockEntityNbt(world.getName(), x, y, z) == null) {
            WorldNatives.createBlockEntity(world.getName(), x, y, z, bytes);
        } else {
            WorldNatives.setBlockEntityNbt(world.getName(), x, y, z, bytes);
        }
    }

    public String text(String key, String fallback) {
        return Nbt.string(data, key, fallback);
    }

    public int integer(String key, int fallback) {
        return Nbt.integer(data, key, fallback);
    }

    public boolean flag(String key, boolean fallback) {
        return Nbt.flag(data, key, fallback);
    }

    public Map<String, Object> map(String key) {
        return Nbt.map(data, key);
    }

    public Map<String, Object> mapOrCreate(String key) {
        return Nbt.mapOrCreate(data, key);
    }

    public void put(String key, Object value) {
        if (value == null) {
            data.remove(key);
        } else {
            data.put(key, value);
        }
    }
}
