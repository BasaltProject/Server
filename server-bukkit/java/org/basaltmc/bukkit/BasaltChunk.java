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
        return new BlockState[0];
    }

    @Override
    public Collection<BlockState> getTileEntities(Predicate<? super Block> blockPredicate, boolean useSnapshot) {
        return List.of();
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
        return false;
    }

    @Override
    public boolean isForceLoaded() {
        return false;
    }

    @Override
    public void setForceLoaded(boolean forced) {}

    @Override
    public boolean addPluginChunkTicket(Plugin plugin) {
        return true;
    }

    @Override
    public boolean removePluginChunkTicket(Plugin plugin) {
        return true;
    }

    @Override
    public Collection<Plugin> getPluginChunkTickets() {
        return List.of();
    }

    @Override
    public long getInhabitedTime() {
        return 0L;
    }

    @Override
    public void setInhabitedTime(long ticks) {}

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
        return false;
    }

    @Override
    public Chunk.LoadLevel getLoadLevel() {
        return isLoaded() ? Chunk.LoadLevel.ENTITY_TICKING : Chunk.LoadLevel.UNLOADED;
    }

    @Override
    public Collection<Player> getPlayersSeeingChunk() {
        return List.of();
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
}
