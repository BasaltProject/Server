package org.basaltmc.bukkit.worldgen;

import org.basaltmc.bukkit.Registries;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.block.Biome;
import org.bukkit.generator.ChunkGenerator;

public final class CellBiomes implements ChunkGenerator.BiomeGrid {
    private final int minY;
    private final int height;
    private final int[] cells;

    CellBiomes(int minY, int maxY, int fallback) {
        this.minY = minY;
        this.height = maxY - minY;
        this.cells = new int[(height >> 2) * 16];
        java.util.Arrays.fill(cells, fallback);
    }

    void load(int[] fromServer) {
        System.arraycopy(fromServer, 0, cells, 0, Math.min(fromServer.length, cells.length));
    }

    int[] cells() {
        return cells;
    }

    private int index(int x, int y, int z) {
        return (((y - minY) >> 2) * 4 + ((z & 15) >> 2)) * 4 + ((x & 15) >> 2);
    }

    private static volatile java.util.Map<String, Integer> kIds;

    private static int idOf(Biome biome) {
        java.util.Map<String, Integer> ids = kIds;
        if (ids == null) {
            String[] all = Registries.biomeIds();
            ids = new java.util.HashMap<>();
            for (int at = all.length - 1; at >= 0; at--) {
                ids.put(all[at], at);
            }
            kIds = ids;
        }
        return ids.getOrDefault(biome.getKey().toString(), -1);
    }

    Biome get(int x, int y, int z) {
        if (y < minY || y >= minY + height) {
            return null;
        }
        String[] ids = Registries.biomeIds();
        int at = cells[index(x, y, z)];
        return at < 0 || at >= ids.length ? null : Registry.BIOME.get(NamespacedKey.fromString(ids[at]));
    }

    void set(int x, int y, int z, Biome biome) {
        int id = idOf(biome);
        if (id >= 0 && y >= minY && y < minY + height) {
            cells[index(x, y, z)] = id;
        }
    }

    @Override
    public Biome getBiome(int x, int z) {
        return get(x, minY, z);
    }

    @Override
    public Biome getBiome(int x, int y, int z) {
        return get(x, y, z);
    }

    @Override
    public void setBiome(int x, int z, Biome biome) {
        for (int y = minY; y < minY + height; y += 4) {
            set(x, y, z, biome);
        }
    }

    @Override
    public void setBiome(int x, int y, int z, Biome biome) {
        set(x, y, z, biome);
    }
}
