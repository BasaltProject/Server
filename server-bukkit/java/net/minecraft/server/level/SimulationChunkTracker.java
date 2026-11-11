// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import it.unimi.dsi.fastutil.longs.Long2ByteMap;
import it.unimi.dsi.fastutil.longs.Long2ByteOpenHashMap;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.TicketStorage;

public class SimulationChunkTracker extends ChunkTracker {
    public static final int MAX_LEVEL = 33;
    protected final Long2ByteMap chunks = new Long2ByteOpenHashMap();
    private final TicketStorage tickets;

    public SimulationChunkTracker(TicketStorage ticketStorage) {
        super(MAX_LEVEL + 1, 16, 256);
        this.tickets = ticketStorage;
        ticketStorage.setSimulationChunkUpdatedListener(this::update);
        chunks.defaultReturnValue((byte) MAX_LEVEL);
    }

    protected int getLevelFromSource(long to) {
        return tickets.getTicketLevelAt(to, true);
    }

    public int getLevel(ChunkPos node) {
        return getLevel(node.pack());
    }

    protected int getLevel(long node) {
        return chunks.get(node);
    }

    protected void setLevel(long node, int level) {
        if (level < MAX_LEVEL) {
            chunks.put(node, (byte) level);
        } else {
            chunks.remove(node);
        }
    }

    public void runAllUpdates() {
        runUpdates(Integer.MAX_VALUE);
    }
}
