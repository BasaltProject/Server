package org.basaltmc.bukkit.worldgen;

import org.basaltmc.bukkit.BasaltBlockData;
import org.basaltmc.bukkit.Registries;
import org.bukkit.HeightMap;
import org.bukkit.Material;
import org.bukkit.block.Biome;
import org.bukkit.block.data.BlockData;
import org.bukkit.generator.ChunkGenerator;
import org.bukkit.material.MaterialData;

public final class BasaltChunkData implements ChunkGenerator.ChunkData {
    private final int minY;
    private final int maxY;
    private final int[][] sections;
    private final CellBiomes biomes;

    BasaltChunkData(int minY, int maxY, CellBiomes biomes) {
        this.minY = minY;
        this.maxY = maxY;
        this.sections = new int[(maxY - minY) >> 4][];
        this.biomes = biomes;
    }

    /** Starts from the blocks of a chunk the server already made, section by section from the lowest. */
    void load(int[] blocks) {
        for (int i = 0; i < sections.length && (i + 1) * 4096 <= blocks.length; i++) {
            int[] one = java.util.Arrays.copyOfRange(blocks, i * 4096, (i + 1) * 4096);
            boolean empty = true;
            for (int state : one) {
                if (state != 0) {
                    empty = false;
                    break;
                }
            }
            sections[i] = empty ? null : one;
        }
    }

    int[] flat() {
        int[] out = new int[sections.length * 4096];
        for (int i = 0; i < sections.length; i++) {
            if (sections[i] != null) {
                System.arraycopy(sections[i], 0, out, i * 4096, 4096);
            }
        }
        return out;
    }

    private int stateOf(int x, int y, int z) {
        if (x < 0 || x > 15 || z < 0 || z > 15 || y < minY || y >= maxY) {
            return 0;
        }
        int[] section = sections[(y - minY) >> 4];
        return section == null ? 0 : section[((y & 15) << 8) | (z << 4) | x];
    }

    private void put(int x, int y, int z, int state) {
        if (x < 0 || x > 15 || z < 0 || z > 15 || y < minY || y >= maxY) {
            return;
        }
        int index = (y - minY) >> 4;
        if (sections[index] == null) {
            if (state == 0) {
                return;
            }
            sections[index] = new int[4096];
        }
        sections[index][((y & 15) << 8) | (z << 4) | x] = state;
    }

    private static int stateOf(Material material) {
        int state = Registries.defaultStateOf(material);
        if (state < 0) {
            throw new IllegalArgumentException(material + " is not a block");
        }
        return state;
    }

    private void fill(int x1, int y1, int z1, int x2, int y2, int z2, int state) {
        for (int x = Math.max(x1, 0); x < Math.min(x2, 16); x++) {
            for (int z = Math.max(z1, 0); z < Math.min(z2, 16); z++) {
                for (int y = Math.max(y1, minY); y < Math.min(y2, maxY); y++) {
                    put(x, y, z, state);
                }
            }
        }
    }

    @Override
    public int getMinHeight() {
        return minY;
    }

    @Override
    public int getMaxHeight() {
        return maxY;
    }

    @Override
    public Biome getBiome(int x, int y, int z) {
        return biomes.get(x, y, z);
    }

    @Override
    public void setBlock(int x, int y, int z, Material material) {
        put(x, y, z, stateOf(material));
    }

    @Override
    public void setBlock(int x, int y, int z, MaterialData data) {
        put(x, y, z, stateOf(data.getItemType()));
    }

    @Override
    public void setBlock(int x, int y, int z, BlockData data) {
        put(x, y, z, BasaltBlockData.stateOf(data));
    }

    @Override
    public void setRegion(int x1, int y1, int z1, int x2, int y2, int z2, Material material) {
        fill(x1, y1, z1, x2, y2, z2, stateOf(material));
    }

    @Override
    public void setRegion(int x1, int y1, int z1, int x2, int y2, int z2, MaterialData data) {
        fill(x1, y1, z1, x2, y2, z2, stateOf(data.getItemType()));
    }

    @Override
    public void setRegion(int x1, int y1, int z1, int x2, int y2, int z2, BlockData data) {
        fill(x1, y1, z1, x2, y2, z2, BasaltBlockData.stateOf(data));
    }

    @Override
    public Material getType(int x, int y, int z) {
        return Registries.materialOfState(stateOf(x, y, z));
    }

    @Override
    public MaterialData getTypeAndData(int x, int y, int z) {
        return new MaterialData(getType(x, y, z));
    }

    @Override
    public BlockData getBlockData(int x, int y, int z) {
        return BasaltBlockData.of(stateOf(x, y, z));
    }

    @Override
    public byte getData(int x, int y, int z) {
        return 0;
    }

    @Override
    public int getHeight(HeightMap heightMap, int x, int z) {
        for (int y = maxY - 1; y >= minY; y--) {
            Material type = getType(x, y, z);
            boolean counts = switch (heightMap) {
                case OCEAN_FLOOR, OCEAN_FLOOR_WG -> type.isSolid();
                case WORLD_SURFACE, WORLD_SURFACE_WG -> !type.isAir();
                default -> type.isSolid() || type == Material.WATER || type == Material.LAVA;
            };
            if (counts) {
                return y;
            }
        }
        return minY - 1;
    }
}
