package org.basaltmc.bukkit.worldedit.fawe;

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
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Collections;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.Future;
import java.util.concurrent.locks.ReentrantLock;
import org.basaltmc.bukkit.WorldNatives;
import org.basaltmc.bukkit.worldedit.TileNbt;

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
        org.basaltmc.bukkit.Blocks.flushAll();
        org.basaltmc.bukkit.Blocks.flushAll();
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
        long t0 = System.nanoTime();
        BasaltGetBlocks copy = createCopy ? new BasaltGetBlocks(world, chunkX, chunkZ, minY, maxY, true) : null;
        int[] previous = copy == null ? null : new int[used * 4096];
        if (used > 0) {
            long t1 = System.nanoTime();
            int changed = WorldNatives.applySections(world, chunkX, chunkZ, java.util.Arrays.copyOf(layers, used),
                java.util.Arrays.copyOf(flat, used * 4096), true, previous);
            if (Boolean.getBoolean("basalt.fawe.trace")) {
                org.basaltmc.bukkit.Natives.log(1, "FAWE-Basalt", "chunk " + chunkX + "," + chunkZ + " sections=" + used
                    + " convert=" + (t1 - t0) / 1000000 + "ms native=" + (System.nanoTime() - t1) / 1000000 + "ms");
            }
            if (changed < 0) {
                org.basaltmc.bukkit.Natives.log(2, "FAWE-Basalt", "chunk " + chunkX + "," + chunkZ + " was not written");
            }
        }
        applyExtras(set);
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
            finalize.run();
        }
        return null;
    }

    private void applyExtras(IChunkSet set) {
        Map<BlockVector3, FaweCompoundTag> tiles = set.tiles();
        if (tiles != null) {
            for (Map.Entry<BlockVector3, FaweCompoundTag> tile : tiles.entrySet()) {
                BlockVector3 at = tile.getKey();
                WorldNatives.setBlockEntityNbt(world, (chunkX << 4) + (at.x() & 15), at.y(), (chunkZ << 4) + (at.z() & 15),
                    TileNbt.write(tile.getValue().linTag()));
            }
        }
        Collection<FaweCompoundTag> entities = set.entities();
        if (entities != null) {
            for (FaweCompoundTag entity : entities) {
                WorldNatives.spawnEntityNbt(world, TileNbt.write(entity.linTag()));
            }
        }
        if (!set.hasBiomes()) {
            return;
        }
        int[] ids = new int[((maxY + 1 - minY) >> 2) * 16];
        java.util.Arrays.fill(ids, -1);
        boolean any = false;
        for (int qy = 0; qy < (maxY + 1 - minY) >> 2; qy++) {
            for (int qz = 0; qz < 4; qz++) {
                for (int qx = 0; qx < 4; qx++) {
                    BiomeType wanted = set.getBiomeType(qx << 2, minY + (qy << 2), qz << 2);
                    if (wanted != null) {
                        ids[(qy * 4 + qz) * 4 + qx] = biomeIndex(wanted);
                        any = true;
                    }
                }
            }
        }
        if (any) {
            WorldNatives.writeBiomes(world, chunkX, chunkZ, ids);
        }
    }

    private static int biomeIndex(BiomeType biome) {
        String[] ids = org.basaltmc.bukkit.Registries.biomeIds();
        for (int i = 0; i < ids.length; i++) {
            if (ids[i].equals(biome.id())) {
                return i;
            }
        }
        return 0;
    }

    @Override
    public Map<BlockVector3, FaweCompoundTag> tiles() {
        int[] where = WorldNatives.blockEntityPositions(world, chunkX, chunkZ);
        Map<BlockVector3, FaweCompoundTag> out = new java.util.HashMap<>();
        for (int i = 0; i + 2 < where.length; i += 3) {
            FaweCompoundTag tag = tile(where[i] & 15, where[i + 1], where[i + 2] & 15);
            if (tag != null) {
                out.put(BlockVector3.at(where[i] & 15, where[i + 1], where[i + 2] & 15), tag);
            }
        }
        return out;
    }

    @Override
    public FaweCompoundTag tile(int x, int y, int z) {
        org.basaltmc.bukkit.Blocks.flushAll();
        byte[] nbt = WorldNatives.blockEntityNbt(world, (chunkX << 4) + x, y, (chunkZ << 4) + z);
        return nbt == null ? null : FaweCompoundTag.of(TileNbt.read(nbt));
    }

    @Override
    public Collection<FaweCompoundTag> entities() {
        List<FaweCompoundTag> out = new ArrayList<>();
        for (int id : WorldNatives.entityIds(world, chunkX, chunkZ, false)) {
            byte[] nbt = WorldNatives.entityNbt(world, id);
            if (nbt != null) {
                out.add(FaweCompoundTag.of(TileNbt.read(nbt)));
            }
        }
        return out;
    }

    @Override
    public FaweCompoundTag entity(UUID uuid) {
        return null;
    }

    @Override
    public Set<Entity> getFullEntities() {
        Set<Entity> out = new java.util.HashSet<>();
        org.bukkit.World bukkit = org.bukkit.Bukkit.getWorld(world);
        for (int id : WorldNatives.entityIds(world, chunkX, chunkZ, false)) {
            org.bukkit.entity.Entity found = org.basaltmc.bukkit.Entities.byId(bukkit, id);
            if (found != null) {
                out.add(com.sk89q.worldedit.bukkit.BukkitAdapter.adapt(found));
            }
        }
        return out;
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
        return copies.remove(key);
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
