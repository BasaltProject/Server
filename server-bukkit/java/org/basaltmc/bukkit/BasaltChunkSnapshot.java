package org.basaltmc.bukkit;

import org.bukkit.Material;
import org.bukkit.block.Biome;
import org.bukkit.block.data.BlockData;

public final class BasaltChunkSnapshot implements org.bukkit.ChunkSnapshot {
    private final BasaltWorld world;
    private final int chunkX;
    private final int chunkZ;
    private final int minY;
    private final int maxY;
    private final int[][] sections;
    private final long captured;

    BasaltChunkSnapshot(BasaltWorld world, int chunkX, int chunkZ, boolean read) {
        this.world = world;
        this.chunkX = chunkX;
        this.chunkZ = chunkZ;
        this.minY = world.getMinHeight();
        this.maxY = world.getMaxHeight();
        this.sections = new int[(maxY - minY) >> 4][];
        this.captured = world.getFullTime();
        if (read) {
            Blocks.flushAll();
            int first = minY >> 4;
            for (int i = 0; i < sections.length; i++) {
                int[] states = new int[4096];
                if (WorldNatives.readSection(world.getName(), chunkX, first + i, chunkZ, true, states)) {
                    sections[i] = states;
                }
            }
        }
    }

    private int state(int x, int y, int z) {
        if (y < minY || y >= maxY) {
            return 0;
        }
        int[] section = sections[(y - minY) >> 4];
        return section == null ? 0 : section[((y & 15) << 8) | ((z & 15) << 4) | (x & 15)];
    }

    @Override
    public int getX() {
        return chunkX;
    }

    @Override
    public int getZ() {
        return chunkZ;
    }

    @Override
    public String getWorldName() {
        return world.getName();
    }

    @Override
    public net.kyori.adventure.key.Key getWorldKey() {
        return world.getKey();
    }

    @Override
    public Material getBlockType(int x, int y, int z) {
        return Registries.materialOfState(state(x, y, z));
    }

    @Override
    public BlockData getBlockData(int x, int y, int z) {
        return BasaltBlockData.of(state(x, y, z));
    }

    @Override
    public int getData(int x, int y, int z) {
        return 0;
    }

    @Override
    public int getBlockSkyLight(int x, int y, int z) {
        return 15;
    }

    @Override
    public int getBlockEmittedLight(int x, int y, int z) {
        return 0;
    }

    @Override
    public int getHighestBlockYAt(int x, int z) {
        for (int y = maxY - 1; y >= minY; y--) {
            if (state(x, y, z) != 0) {
                return y;
            }
        }
        return minY;
    }

    @Override
    public Biome getBiome(int x, int z) {
        return world.getBiome((chunkX << 4) + x, minY, (chunkZ << 4) + z);
    }

    @Override
    public Biome getBiome(int x, int y, int z) {
        return world.getBiome((chunkX << 4) + x, y, (chunkZ << 4) + z);
    }

    @Override
    public double getRawBiomeTemperature(int x, int z) {
        return 0.8;
    }

    @Override
    public double getRawBiomeTemperature(int x, int y, int z) {
        return 0.8;
    }

    @Override
    public long getCaptureFullTime() {
        return captured;
    }

    @Override
    public boolean isSectionEmpty(int section) {
        int[] states = section >= 0 && section < sections.length ? sections[section] : null;
        if (states == null) {
            return true;
        }
        for (int state : states) {
            if (state != 0) {
                return false;
            }
        }
        return true;
    }

    @Override
    public boolean contains(BlockData data) {
        int wanted = BasaltBlockData.stateOf(data);
        for (int[] states : sections) {
            if (states != null) {
                for (int state : states) {
                    if (state == wanted) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    @Override
    public boolean contains(Biome biome) {
        return false;
    }
}
