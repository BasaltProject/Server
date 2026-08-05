package org.basaltmc.bukkit.tile;

import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import org.basaltmc.bukkit.WorldEnvNatives;
import org.bukkit.Bukkit;

final class Lids {
    private static final Set<String> kOpen = ConcurrentHashMap.newKeySet();

    private Lids() {}

    private static String key(Tile tile) {
        return tile.world.getName() + ":" + tile.x + "," + tile.y + "," + tile.z;
    }

    static Object set(Tile tile, boolean open) {
        String world = tile.world.getName();
        boolean barrel = tile.block.equals("barrel");
        if (barrel) {
            String text = tile.live().getBlockData().getAsString().replaceAll("open=(true|false)", "open=" + open);
            tile.live().setBlockData(Bukkit.createBlockData(text));
        } else {
            WorldEnvNatives.blockEvent(world, tile.x, tile.y, tile.z, 1, open ? 1 : 0);
        }
        String sound = tile.block.endsWith("shulker_box") ? (open ? "block.shulker_box.open" : "block.shulker_box.close")
            : barrel ? (open ? "block.barrel.open" : "block.barrel.close")
            : tile.block.equals("ender_chest") ? (open ? "block.ender_chest.open" : "block.ender_chest.close")
            : open ? "block.chest.open" : "block.chest.close";
        WorldEnvNatives.sound(world, "minecraft:" + sound, "blocks", tile.x + 0.5, tile.y + 0.5, tile.z + 0.5, 0.5F,
            0.9F, 0L);
        if (open) {
            kOpen.add(key(tile));
        } else {
            kOpen.remove(key(tile));
        }
        return null;
    }

    static boolean isOpen(Tile tile) {
        if (tile.block.equals("barrel")) {
            return "true".equals(tile.property("open"));
        }
        return kOpen.contains(key(tile));
    }
}
