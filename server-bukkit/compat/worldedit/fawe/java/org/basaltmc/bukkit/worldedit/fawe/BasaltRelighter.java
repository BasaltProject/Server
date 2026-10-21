package org.basaltmc.bukkit.worldedit.fawe;

import com.fastasyncworldedit.core.extent.processor.lighting.Relighter;
import java.util.concurrent.locks.ReentrantLock;

/** Basalt marks the light of a changed chunk stale and recomputes it before the chunk is sent, so FAWE has no work. */
final class BasaltRelighter implements Relighter {
    private final ReentrantLock lock = new ReentrantLock();

    @Override
    public boolean addChunk(int cx, int cz, byte[] skipReason, int bitmask) {
        return true;
    }

    @Override
    public void addLightUpdate(int x, int y, int z) {}

    @Override
    public void fixLightingSafe(boolean sky) {}

    @Override
    public void removeAndRelight(boolean sky) {}

    @Override
    public void clear() {}

    @Override
    public void removeLighting() {}

    @Override
    public void fixBlockLighting() {}

    @Override
    public void fixSkyLighting() {}

    @Override
    public boolean isEmpty() {
        return true;
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
