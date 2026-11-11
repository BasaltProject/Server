// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import net.minecraft.world.level.TicketStorage;

public class LoadingChunkTracker extends ChunkTracker {
    private static final int MAX_LEVEL = ChunkLevel.MAX_LEVEL + 1;
    private final DistanceManager distanceManager;
    private final TicketStorage tickets;

    public LoadingChunkTracker(DistanceManager distanceManager, TicketStorage ticketStorage) {
        super(MAX_LEVEL + 1, 16, 256);
        this.distanceManager = distanceManager;
        this.tickets = ticketStorage;
        ticketStorage.setLoadingChunkUpdatedListener(this::update);
    }

    protected int getLevelFromSource(long to) {
        return tickets.getTicketLevelAt(to, false);
    }

    protected int getLevel(long node) {
        if (distanceManager.isChunkToRemove(node)) {
            return MAX_LEVEL;
        }
        ChunkHolder holder = distanceManager.getChunk(node);
        return holder == null ? MAX_LEVEL : holder.getTicketLevel();
    }

    protected void setLevel(long node, int level) {
        ChunkHolder holder = distanceManager.getChunk(node);
        int previous = holder == null ? MAX_LEVEL : holder.getTicketLevel();
        if (previous == level) {
            return;
        }
        holder = distanceManager.updateChunkScheduling(node, level, holder, previous);
        if (holder != null) {
            distanceManager.chunksToUpdateFutures.add(holder);
        }
    }

    public int runDistanceUpdates(int count) {
        return runUpdates(count);
    }
}
