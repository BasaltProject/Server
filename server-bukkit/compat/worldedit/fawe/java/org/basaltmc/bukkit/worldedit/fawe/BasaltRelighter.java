package org.basaltmc.bukkit.worldedit.fawe;

import com.fastasyncworldedit.core.extent.processor.lighting.Relighter;
import java.util.concurrent.locks.ReentrantLock;

/** Basalt marks the light of a changed chunk stale itself; chunks FAWE asks to relight are marked and shown again. */
final class BasaltRelighter implements Relighter {
    private final ReentrantLock lock = new ReentrantLock();
    private final String world;
    private final java.util.Set<Long> chunks = java.util.concurrent.ConcurrentHashMap.newKeySet();

    BasaltRelighter(String world) {
        this.world = world;
    }

    @Override
    public boolean addChunk(int cx, int cz, byte[] skipReason, int bitmask) {
        chunks.add(((long) cx << 32) | (cz & 0xFFFFFFFFL));
        return true;
    }

    private void relight() {
        if (chunks.isEmpty()) {
            return;
        }
        int[] pairs = new int[chunks.size() * 2];
        int at = 0;
        for (long chunk : chunks) {
            pairs[at++] = (int) (chunk >> 32);
            pairs[at++] = (int) chunk;
        }
        chunks.clear();
        org.basaltmc.bukkit.WorldNatives.relightChunks(world, pairs);
    }

    @Override
    public void addLightUpdate(int x, int y, int z) {}

    @Override
    public void fixLightingSafe(boolean sky) {
        relight();
    }

    @Override
    public void removeAndRelight(boolean sky) {
        relight();
    }

    @Override
    public void clear() {}

    @Override
    public void removeLighting() {}

    @Override
    public void fixBlockLighting() {
        relight();
    }

    @Override
    public void fixSkyLighting() {
        relight();
    }

    @Override
    public boolean isEmpty() {
        return chunks.isEmpty();
    }

    @Override
    public ReentrantLock getLock() {
        return lock;
    }

    @Override
    public boolean isFinished() {
        return true;
    }

    @Override
    public void close() {}
}
