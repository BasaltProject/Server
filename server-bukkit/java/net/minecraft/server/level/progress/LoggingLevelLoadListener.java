// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level.progress;

import net.minecraft.network.chat.Component;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.Mth;
import net.minecraft.util.Util;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Level;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class LoggingLevelLoadListener implements LevelLoadListener {
    private static final Logger LOG = LoggerFactory.getLogger(LoggingLevelLoadListener.class);
    private static final long LOG_INTERVAL_MS = 500L;
    private final boolean includePlayerChunks;
    private final LevelLoadProgressTracker tracker;
    private boolean closed;
    private long startedAt = Long.MAX_VALUE;
    private long nextLogAt = Long.MAX_VALUE;

    public LoggingLevelLoadListener(boolean includePlayerChunks) {
        this.includePlayerChunks = includePlayerChunks;
        this.tracker = new LevelLoadProgressTracker(includePlayerChunks);
    }

    public static LoggingLevelLoadListener forDedicatedServer() {
        return new LoggingLevelLoadListener(false);
    }

    public static LoggingLevelLoadListener forSingleplayer() {
        return new LoggingLevelLoadListener(true);
    }

    public void start(LevelLoadListener.Stage stage, int totalChunks) {
        if (closed) {
            return;
        }
        if (startedAt == Long.MAX_VALUE) {
            startedAt = Util.getMillis();
            nextLogAt = startedAt;
        }
        tracker.start(stage, totalChunks);
        if (stage == LevelLoadListener.Stage.PREPARE_GLOBAL_SPAWN) {
            LOG.info("Selecting global world spawn...");
        } else if (stage == LevelLoadListener.Stage.LOAD_INITIAL_CHUNKS) {
            LOG.info("Loading {} persistent chunks...", totalChunks);
        } else if (stage == LevelLoadListener.Stage.LOAD_PLAYER_CHUNKS) {
            LOG.info("Loading {} chunks for player spawn...", totalChunks);
        }
    }

    public void update(LevelLoadListener.Stage stage, int currentChunks, int totalChunks) {
        if (closed) {
            return;
        }
        tracker.update(stage, currentChunks, totalChunks);
        if (Util.getMillis() > nextLogAt) {
            nextLogAt += LOG_INTERVAL_MS;
            LOG.info("{}", Component.translatable("menu.preparingSpawn", Mth.floor(tracker.get() * 100.0F)).getString());
        }
    }

    public void finish(LevelLoadListener.Stage stage) {
        if (closed) {
            return;
        }
        tracker.finish(stage);
        LevelLoadListener.Stage last = includePlayerChunks ? LevelLoadListener.Stage.LOAD_PLAYER_CHUNKS : LevelLoadListener.Stage.LOAD_INITIAL_CHUNKS;
        if (stage == last) {
            LOG.info("Time elapsed: {} ms", Util.getMillis() - startedAt);
            nextLogAt = Long.MAX_VALUE;
            closed = true;
        }
    }

    public void updateFocus(ResourceKey<Level> dimension, ChunkPos chunkPos) {}
}
