package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.function.Predicate;
import org.bukkit.Chunk;
import org.bukkit.World;
import org.bukkit.block.Biome;
import org.bukkit.block.Block;
import org.bukkit.block.BlockState;
import org.bukkit.block.data.BlockData;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Player;
import org.bukkit.plugin.Plugin;
import org.basaltmc.bukkit.gen.ChunkBase;

public final class BasaltChunk extends ChunkBase {
    private final BasaltWorld world;
    private final int x;
    private final int z;

    BasaltChunk(BasaltWorld world, int x, int z) {
        this.world = world;
        this.x = x;
        this.z = z;
    }

    @Override
    public int getX() {
        return x;
    }

    @Override
    public net.minecraft.world.level.chunk.ChunkAccess getHandle(net.minecraft.world.level.chunk.status.ChunkStatus status) {
        return world.getHandle().getChunk(x, z, status, true);
    }

    @Override
    public int getZ() {
        return z;
    }

    @Override
    public World getWorld() {
        return world;
    }

    @Override
    public Block getBlock(int blockX, int blockY, int blockZ) {
        return world.getBlockAt((x << 4) + (blockX & 15), blockY, (z << 4) + (blockZ & 15));
    }

    @Override
    public org.bukkit.ChunkSnapshot getChunkSnapshot(boolean includeMaxBlockY, boolean includeBiome,
            boolean includeBiomeTempRain, boolean includeLightData) {
        return new BasaltChunkSnapshot(world, x, z, true);
    }

    @Override
    public boolean isEntitiesLoaded() {
        return isLoaded();
    }

    @Override
    public Entity[] getEntities() {
        List<Entity> found = new ArrayList<>();
        for (int id : WorldNatives.entityIds(world.getName(), x, z, false)) {
            Entity entity = world.entity(id);
            if (entity != null) {
                found.add(entity);
            }
        }
        return found.toArray(new Entity[0]);
    }

    @Override
    public BlockState[] getTileEntities(boolean useSnapshot) {
        return getTileEntities(block -> true, useSnapshot).toArray(new BlockState[0]);
    }

    @Override
    public Collection<BlockState> getTileEntities(Predicate<? super Block> blockPredicate, boolean useSnapshot) {
        Blocks.flushAll();
        List<BlockState> out = new java.util.ArrayList<>();
        int[] at = WorldNatives.blockEntityPositions(world.getName(), x, z);
        for (int i = 0; at != null && i + 2 < at.length; i += 3) {
            Block block = world.getBlockAt(at[i], at[i + 1], at[i + 2]);
            if (blockPredicate.test(block)) {
                out.add(block.getState());
            }
        }
        return out;
    }

    @Override
    public boolean isGenerated() {
        return isLoaded();
    }

    @Override
    public boolean isLoaded() {
        return WorldNatives.chunkLoaded(world.getName(), x, z);
    }

    @Override
    public boolean load(boolean generate) {
        return WorldNatives.loadChunk(world.getName(), x, z, generate);
    }

    @Override
    public boolean load() {
        return load(true);
    }

    @Override
    public boolean unload(boolean save) {
        Blocks.flushAll();
        return WorldNatives.unloadChunk(world.getName(), x, z);
    }

    @Override
    public boolean unload() {
        return unload(true);
    }

    @Override
    public boolean isSlimeChunk() {
        long seed = world.getSeed() + (long) (x * x * 0x4c1906) + (long) (x * 0x5ac0db) + (long) (z * z) * 0x4307a7L
            + (long) (z * 0x5f24f) ^ 0x3ad8025fL;
        return new java.util.Random(seed).nextInt(10) == 0;
    }

    @Override
    public boolean isForceLoaded() {
        return world.isChunkForceLoaded(x, z);
    }

    @Override
    public void setForceLoaded(boolean forced) {
        world.setChunkForceLoaded(x, z, forced);
    }

    @Override
    public boolean addPluginChunkTicket(Plugin plugin) {
        return world.addPluginChunkTicket(x, z, plugin);
    }

    @Override
    public boolean removePluginChunkTicket(Plugin plugin) {
        return world.removePluginChunkTicket(x, z, plugin);
    }

    @Override
    public Collection<Plugin> getPluginChunkTickets() {
        return world.getPluginChunkTickets(x, z);
    }

    @Override
    public long getInhabitedTime() {
        return WorldEnvNatives.inhabitedTime(world.getName(), x, z, -1L);
    }

    @Override
    public void setInhabitedTime(long ticks) {
        WorldEnvNatives.inhabitedTime(world.getName(), x, z, Math.max(ticks, 0L));
    }

    @Override
    public Collection<org.bukkit.generator.structure.GeneratedStructure> getStructures() {
        return world.getStructures(x, z);
    }

    @Override
    public Collection<org.bukkit.generator.structure.GeneratedStructure> getStructures(
        org.bukkit.generator.structure.Structure structure) {
        return world.getStructures(x, z, structure);
    }

    @Override
    public boolean contains(BlockData data) {
        int wanted = BasaltBlockData.stateOf(data);
        int[] states = new int[4096];
        int minSection = world.getMinHeight() >> 4;
        for (int section = 0; section < world.getMaxHeight() - world.getMinHeight() >> 4; section++) {
            if (WorldNatives.readSection(world.getName(), x, minSection + section, z, false, states)) {
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
        String[] known = Registries.biomeIds();
        int wanted = java.util.Arrays.asList(known).indexOf(biome.getKey().toString());
        int[] cells = org.basaltmc.bukkit.facade.Chunks.readBiomes(world.getName(), x, z,
            ((world.getMaxHeight() - world.getMinHeight()) >> 4) * 64);
        for (int cell : cells == null ? new int[0] : cells) {
            if (cell == wanted) {
                return true;
            }
        }
        return false;
    }

    @Override
    public Chunk.LoadLevel getLoadLevel() {
        return isLoaded() ? Chunk.LoadLevel.ENTITY_TICKING : Chunk.LoadLevel.UNLOADED;
    }

    @Override
    public Collection<Player> getPlayersSeeingChunk() {
        return world.getPlayersSeeingChunk(x, z);
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltChunk chunk && chunk.x == x && chunk.z == z && chunk.world.equals(world);
    }

    @Override
    public int hashCode() {
        return (x * 31 + z) * 31 + world.hashCode();
    }

    @Override
    public String toString() {
        return "BasaltChunk{world=" + world.getName() + ",x=" + x + ",z=" + z + "}";
    }

    @Override
    public org.bukkit.persistence.PersistentDataContainer getPersistentDataContainer() {
        return org.basaltmc.bukkit.persistence.PdcStores.chunk(world.getName(), x, z);
    }
}
