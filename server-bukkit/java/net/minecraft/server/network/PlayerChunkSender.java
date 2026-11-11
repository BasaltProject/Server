// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import it.unimi.dsi.fastutil.longs.LongOpenHashSet;
import it.unimi.dsi.fastutil.longs.LongSet;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import net.minecraft.network.protocol.game.ClientboundChunkBatchFinishedPacket;
import net.minecraft.network.protocol.game.ClientboundChunkBatchStartPacket;
import net.minecraft.network.protocol.game.ClientboundForgetLevelChunkPacket;
import net.minecraft.network.protocol.game.ClientboundLevelChunkWithLightPacket;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.util.Mth;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.LevelChunk;

public class PlayerChunkSender {
    public static final float MIN_CHUNKS_PER_TICK = 0.01F;
    public static final float MAX_CHUNKS_PER_TICK = 64.0F;
    private static final float START_RATE = 9.0F;
    private static final int MAX_IN_FLIGHT = 10;
    private final LongSet pending = new LongOpenHashSet();
    private final boolean memoryConnection;
    private float rate = START_RATE;
    private float quota;
    private int inFlight;
    private int inFlightLimit = 1;

    public PlayerChunkSender(boolean memoryConnection) {
        this.memoryConnection = memoryConnection;
    }

    public void markChunkPendingToSend(LevelChunk chunk) {
        pending.add(chunk.getPos().pack());
    }

    public void dropChunk(ServerPlayer player, ChunkPos pos) {
        if (!pending.remove(pos.pack()) && player.isAlive()) {
            player.connection.send(new ClientboundForgetLevelChunkPacket(pos));
        }
    }

    public void sendNextChunks(ServerPlayer player) {
        if (inFlight >= inFlightLimit) {
            return;
        }
        quota = Math.min(quota + rate, Math.max(1.0F, rate));
        if (quota < 1.0F || pending.isEmpty()) {
            return;
        }
        ServerLevel level = player.level();
        List<LevelChunk> batch = pickBatch(level.getChunkSource().chunkMap, player.chunkPosition());
        if (batch.isEmpty()) {
            return;
        }
        inFlight++;
        player.connection.send(ClientboundChunkBatchStartPacket.INSTANCE);
        for (LevelChunk chunk : batch) {
            player.connection.send(new ClientboundLevelChunkWithLightPacket(chunk, level.getLightEngine(), null, null));
        }
        player.connection.send(new ClientboundChunkBatchFinishedPacket(batch.size()));
        quota -= batch.size();
    }

    private List<LevelChunk> pickBatch(net.minecraft.server.level.ChunkMap chunkMap, ChunkPos center) {
        int limit = Mth.floor(quota);
        long[] keys = pending.toLongArray();
        List<LevelChunk> ready = new ArrayList<>();
        for (long key : keys) {
            LevelChunk chunk = chunkMap.getChunkToSend(key);
            if (chunk != null) {
                ready.add(chunk);
            }
        }
        ready.sort(Comparator.comparingInt(chunk -> center.distanceSquared(chunk.getPos())));
        List<LevelChunk> chosen = !memoryConnection && ready.size() > limit ? new ArrayList<>(ready.subList(0, limit)) : ready;
        chosen.forEach(chunk -> pending.remove(chunk.getPos().pack()));
        return chosen;
    }

    public void onChunkBatchReceivedByClient(float desiredChunksPerTick) {
        inFlight = Math.max(0, inFlight - 1);
        rate = Float.isNaN(desiredChunksPerTick) ? MIN_CHUNKS_PER_TICK : Mth.clamp(desiredChunksPerTick, MIN_CHUNKS_PER_TICK, MAX_CHUNKS_PER_TICK);
        if (inFlight == 0) {
            quota = 1.0F;
        }
        inFlightLimit = MAX_IN_FLIGHT;
    }

    public boolean isPending(long pos) {
        return pending.contains(pos);
    }
}
