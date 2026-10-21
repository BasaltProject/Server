package org.basaltmc.bukkit.fawe;

import com.fastasyncworldedit.core.nbt.FaweCompoundTag;
import com.fastasyncworldedit.core.queue.IChunk;
import com.fastasyncworldedit.core.queue.IChunkGet;
import com.fastasyncworldedit.core.queue.IChunkSet;
import com.fastasyncworldedit.core.queue.IQueueExtent;
import com.fastasyncworldedit.core.queue.implementation.blocks.CharGetBlocks;
import com.sk89q.worldedit.entity.Entity;
import com.sk89q.worldedit.math.BlockVector3;
import com.sk89q.worldedit.world.biome.BiomeType;
import com.sk89q.worldedit.world.biome.BiomeTypes;
import com.sk89q.worldedit.world.block.BlockTypesCache;
import com.sk89q.worldedit.world.block.BlockState;
import java.util.Collection;
import java.util.Collections;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.Future;
import java.util.concurrent.locks.ReentrantLock;
import org.basaltmc.bukkit.WorldNatives;

/** One chunk of a Basalt world as FAWE sees it: sections are read and written whole through the native services. */
final class BasaltGetBlocks extends CharGetBlocks {
    private final String world;
    private final int chunkX;
    private final int chunkZ;
    private final int minY;
    private final int maxY;
    private final ReentrantLock callLock = new ReentrantLock();
    private final long[] stamps;
    private final boolean frozen;
    private final java.util.concurrent.ConcurrentHashMap<Integer, IChunkGet> copies =
        new java.util.concurrent.ConcurrentHashMap<>();
    private boolean createCopy;
    private int copyKey;

    BasaltGetBlocks(String world, int chunkX, int chunkZ, int minY, int maxY) {
        this(world, chunkX, chunkZ, minY, maxY, false);
    }

    /** A frozen chunk holds what was in the world when it was made and is never refreshed. */
    private BasaltGetBlocks(String world, int chunkX, int chunkZ, int minY, int maxY, boolean frozen) {
        super(minY >> 4, maxY >> 4);
        this.frozen = frozen;
        this.world = world;
        this.chunkX = chunkX;
        this.chunkZ = chunkZ;
        this.minY = minY;
        this.maxY = maxY;
        this.stamps = new long[sectionCount];
    }

    /** A section read from the world is kept briefly: the world changes under it, and FAWE keeps these around. */
    private static final long kLifetimeNanos = 200_000_000L;

    private void expire(int layer) {
        int index = layer - minSectionPosition;
        if (frozen || index < 0 || index >= sectionCount || blocks[index] == null) {
            return;
        }
        synchronized (sectionLocks[index]) {
            if (blocks[index] != null && System.nanoTime() - stamps[index] > kLifetimeNanos) {
                blocks[index] = null;
            }
        }
    }

    @Override
    public char get(int x, int y, int z) {
        expire(y >> 4);
        return super.get(x, y, z);
    }

    @Override
    public boolean hasSection(int layer) {
        if (frozen) {
            return layer >= minSectionPosition && layer <= maxSectionPosition && blocks[layer - minSectionPosition] != null;
        }
        return layer >= minSectionPosition && layer <= maxSectionPosition;
    }

    @Override
    public char[] load(int layer) {
        org.basaltmc.bukkit.Natives.log(1, "FAWE-Basalt", "load " + layer + " frozen=" + frozen);
        expire(layer);
        return super.load(layer);
    }

    @Override
    public char[] loadIfPresent(int layer) {
        expire(layer);
        return super.loadIfPresent(layer);
    }

    @Override
    public char[] update(int layer, char[] data, boolean aggressive) {
        if (data == null) {
            data = new char[4096];
        }
        int[] states = new int[4096];
        if (!WorldNatives.readSection(world, chunkX, layer + minSectionPosition, chunkZ, true, states)) {
            java.util.Arrays.fill(data, (char) 1);
            return data;
        }
        for (int i = 0; i < 4096; i++) {
            data[i] = (char) StateTable.toOrdinal(states[i]);
        }
        stamps[layer] = System.nanoTime();
        return data;
    }

    @Override
    public BiomeType getBiomeType(int x, int y, int z) {
        String id = WorldNatives.biome(world, (chunkX << 4) + x, y, (chunkZ << 4) + z);
        BiomeType found = id == null ? null : BiomeType.REGISTRY.get(id);
        return found == null ? BiomeTypes.PLAINS : found;
    }

    @Override
    public int getSkyLight(int x, int y, int z) {
        return 15;
    }

    @Override
    public int getEmittedLight(int x, int y, int z) {
        return 0;
    }

    @Override
    public int[] getHeightMap(com.fastasyncworldedit.core.extent.processor.heightmap.HeightMapType type) {
        int[] heights = new int[256];
        String kind = switch (type) {
            case OCEAN_FLOOR -> "ocean_floor";
            case WORLD_SURFACE -> "surface";
            default -> "motion_blocking";
        };
        for (int i = 0; i < 256; i++) {
            heights[i] = WorldNatives.highest(world, (chunkX << 4) + (i & 15), (chunkZ << 4) + (i >> 4), kind, true);
        }
        return heights;
    }

    @Override
    @SuppressWarnings("unchecked")
    public <T extends Future<T>> T call(IQueueExtent<? extends IChunk> owner, IChunkSet set, Runnable finalize) {
        int[] layers = new int[sectionCount];
        int[] flat = new int[sectionCount * 4096];
        int used = 0;
        for (int layer = minSectionPosition; layer <= maxSectionPosition; layer++) {
            if (!set.hasSection(layer)) {
                continue;
            }
            char[] ordinals = set.load(layer);
            int base = used * 4096;
            boolean any = false;
            for (int i = 0; i < 4096; i++) {
                char ordinal = ordinals[i];
                if (ordinal == 0) {
                    flat[base + i] = -1;
                } else {
                    flat[base + i] = StateTable.toState(ordinal);
                    any = true;
                }
            }
            if (any) {
                layers[used++] = layer;
            }
        }
        org.basaltmc.bukkit.Natives.log(1, "FAWE-Basalt", "call " + chunkX + "," + chunkZ + " copy=" + createCopy + " key=" + copyKey);
        BasaltGetBlocks copy = createCopy ? new BasaltGetBlocks(world, chunkX, chunkZ, minY, maxY, true) : null;
        int[] previous = copy == null ? null : new int[used * 4096];
        if (used > 0) {
            int changed = WorldNatives.applySections(world, chunkX, chunkZ, java.util.Arrays.copyOf(layers, used),
                java.util.Arrays.copyOf(flat, used * 4096), true, previous);
            if (changed < 0) {
                org.basaltmc.bukkit.Natives.log(2, "FAWE-Basalt", "chunk " + chunkX + "," + chunkZ + " was not written");
            }
        }
        if (copy != null) {
            for (int i = 0; i < used; i++) {
                char[] ordinals = new char[4096];
                for (int j = 0; j < 4096; j++) {
                    ordinals[j] = (char) StateTable.toOrdinal(previous[i * 4096 + j]);
                }
                copy.blocks[layers[i] - minSectionPosition] = ordinals;
            }
            copies.put(copyKey, copy);
        }
        reset();
        if (finalize != null) {
            try {
                finalize.run();
            } catch (Throwable t) {
                java.io.StringWriter trace = new java.io.StringWriter();
                t.printStackTrace(new java.io.PrintWriter(trace));
                org.basaltmc.bukkit.Natives.log(3, "FAWE-Basalt", "finalize threw " + trace);
                throw t;
            }
        }
        return null;
    }

    @Override
    public Map<BlockVector3, FaweCompoundTag> tiles() {
        org.basaltmc.bukkit.Natives.log(1, "FAWE-Basalt", "tiles frozen=" + frozen);
        return Collections.emptyMap();
    }

    @Override
    public FaweCompoundTag tile(int x, int y, int z) {
        return null;
    }

    @Override
    public Collection<FaweCompoundTag> entities() {
        return Collections.emptyList();
    }

    @Override
    public FaweCompoundTag entity(UUID uuid) {
        return null;
    }

    @Override
    public Set<Entity> getFullEntities() {
        return Collections.emptySet();
    }

    @Override
    public boolean isCreateCopy() {
        return createCopy;
    }

    @Override
    public int setCreateCopy(boolean createCopy) {
        this.createCopy = createCopy;
        return ++copyKey;
    }

    @Override
    public IChunkGet getCopy(int key) {
        IChunkGet real = copies.remove(key);
        if (real == null) {
            return null;
        }
        return (IChunkGet) java.lang.reflect.Proxy.newProxyInstance(IChunkGet.class.getClassLoader(),
            new Class<?>[] {IChunkGet.class}, (proxy, method, args) -> {
                org.basaltmc.bukkit.Natives.log(1, "FAWE-Basalt", "copy." + method.getName());
                try {
                    return method.invoke(real, args);
                } catch (java.lang.reflect.InvocationTargetException e) {
                    org.basaltmc.bukkit.Natives.log(3, "FAWE-Basalt", "copy." + method.getName() + " threw " + e.getCause());
                    throw e.getCause();
                }
            });
    }

    @Override
    public void lockCall() {
        callLock.lock();
    }

    @Override
    public void unlockCall() {
        callLock.unlock();
    }

    @Override
    public void setLightingToGet(char[][] lighting, int startSectionIndex, int endSectionIndex) {}

    @Override
    public void setSkyLightingToGet(char[][] lighting, int startSectionIndex, int endSectionIndex) {}

    @Override
    public void setHeightmapToGet(com.fastasyncworldedit.core.extent.processor.heightmap.HeightMapType type, int[] data) {}

    @Override
    public int getMaxY() {
        return maxY;
    }

    @Override
    public int getMinY() {
        return minY;
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
    public void removeSectionLighting(int layer, boolean sky) {}

    @Override
    public BlockState getBlock(int x, int y, int z) {
        return BlockTypesCache.states[get(x, y, z)];
    }
}
