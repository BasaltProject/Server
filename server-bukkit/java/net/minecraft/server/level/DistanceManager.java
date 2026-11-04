// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import it.unimi.dsi.fastutil.longs.LongArrayList;
import it.unimi.dsi.fastutil.longs.LongIterator;
import java.util.concurrent.Executor;
import java.util.function.LongConsumer;
import net.minecraft.core.SectionPos;
import net.minecraft.util.TriState;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.TicketStorage;

public abstract class DistanceManager {
    protected final java.util.Set<ChunkHolder> chunksToUpdateFutures = new java.util.HashSet<>();
    private org.bukkit.World world;

    protected DistanceManager(TicketStorage ticketStorage, Executor executor, Executor mainThreadExecutor) {
    }

    protected abstract boolean isChunkToRemove(long node);

    protected abstract ChunkHolder getChunk(long node);

    protected abstract ChunkHolder updateChunkScheduling(long node, int level, ChunkHolder chunk, int oldLevel);

    void bind(org.bukkit.World world) {
        this.world = world;
    }

    private int nearestPlayerDistance(int chunkX, int chunkZ) {
        int best = Integer.MAX_VALUE;
        if (world == null) {
            return best;
        }
        for (org.bukkit.entity.Player player : world.getPlayers()) {
            if (player.getGameMode() == org.bukkit.GameMode.SPECTATOR) {
                continue;
            }
            int dx = Math.abs((player.getLocation().getBlockX() >> 4) - chunkX);
            int dz = Math.abs((player.getLocation().getBlockZ() >> 4) - chunkZ);
            best = Math.min(best, Math.max(dx, dz));
        }
        return best;
    }

    private boolean withinSimulation(long key) {
        ChunkPos pos = ChunkPos.unpack(key);
        return world != null && world.isChunkLoaded(pos.x(), pos.z())
            && nearestPlayerDistance(pos.x(), pos.z()) <= world.getSimulationDistance();
    }

    public boolean runAllUpdates(ChunkMap scheduler) {
        return false;
    }

    public void addPlayer(SectionPos pos, ServerPlayer player) {
    }

    public void removePlayer(SectionPos pos, ServerPlayer player) {
    }

    public boolean inEntityTickingRange(long key) {
        return withinSimulation(key);
    }

    public boolean inBlockTickingRange(long key) {
        return withinSimulation(key);
    }

    public int getChunkLevel(long key, boolean simulation) {
        ChunkPos pos = ChunkPos.unpack(key);
        if (world == null || !world.isChunkLoaded(pos.x(), pos.z())) {
            return 66;
        }
        return withinSimulation(key) ? 31 : 33;
    }

    protected void updatePlayerTickets(int viewDistance) {
    }

    public void updateSimulationDistance(int newDistance) {
        if (world != null) {
            world.setSimulationDistance(newDistance);
        }
    }

    public int getNaturalSpawnChunkCount() {
        int count = 0;
        if (world != null) {
            for (org.bukkit.Chunk chunk : world.getLoadedChunks()) {
                if (withinSimulation(chunk.getChunkKey())) {
                    count++;
                }
            }
        }
        return count;
    }

    public TriState hasPlayersNearby(long pos) {
        return withinSimulation(pos) ? TriState.TRUE : TriState.FALSE;
    }

    public void forEachEntityTickingChunk(LongConsumer consumer) {
        if (world != null) {
            for (org.bukkit.Chunk chunk : world.getLoadedChunks()) {
                if (withinSimulation(chunk.getChunkKey())) {
                    consumer.accept(chunk.getChunkKey());
                }
            }
        }
    }

    public LongIterator getSpawnCandidateChunks() {
        LongArrayList keys = new LongArrayList();
        forEachEntityTickingChunk(keys::add);
        return keys.iterator();
    }

    public String getDebugStatus() {
        return world == null ? "0" : String.valueOf(world.getLoadedChunks().length);
    }

    public boolean hasTickets() {
        return world != null && !world.getPluginChunkTickets().isEmpty();
    }
}
