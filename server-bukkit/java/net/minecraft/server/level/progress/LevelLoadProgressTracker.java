// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level.progress;

import net.minecraft.resources.ResourceKey;
import net.minecraft.util.Mth;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Level;

public class LevelLoadProgressTracker implements LevelLoadListener {
    private static final int PREPARE_SERVER_WEIGHT = 10;
    private static final int EXPECTED_PLAYER_CHUNKS = Mth.square(7);
    private final boolean includePlayerChunks;
    private int totalWeight;
    private int doneWeight;
    private int currentWeight;
    private float currentFraction;
    private volatile float progress;

    public LevelLoadProgressTracker(boolean includePlayerChunks) {
        this.includePlayerChunks = includePlayerChunks;
    }

    public void start(LevelLoadListener.Stage stage, int totalChunks) {
        if (!tracks(stage)) {
            return;
        }
        if (stage == LevelLoadListener.Stage.LOAD_INITIAL_CHUNKS) {
            totalWeight = PREPARE_SERVER_WEIGHT + totalChunks + (includePlayerChunks ? EXPECTED_PLAYER_CHUNKS : 0);
            beginSegment(PREPARE_SERVER_WEIGHT);
            finishSegment();
            beginSegment(totalChunks);
        } else if (stage == LevelLoadListener.Stage.LOAD_PLAYER_CHUNKS) {
            beginSegment(EXPECTED_PLAYER_CHUNKS);
        }
    }

    private void beginSegment(int weight) {
        currentWeight = weight;
        currentFraction = 0.0F;
        refresh();
    }

    public void update(LevelLoadListener.Stage stage, int currentChunks, int totalChunks) {
        if (tracks(stage)) {
            currentFraction = totalChunks == 0 ? 0.0F : (float) currentChunks / (float) totalChunks;
            refresh();
        }
    }

    public void finish(LevelLoadListener.Stage stage) {
        if (tracks(stage)) {
            finishSegment();
        }
    }

    private void finishSegment() {
        doneWeight += currentWeight;
        currentWeight = 0;
        refresh();
    }

    private boolean tracks(LevelLoadListener.Stage stage) {
        return stage == LevelLoadListener.Stage.LOAD_INITIAL_CHUNKS || stage == LevelLoadListener.Stage.LOAD_PLAYER_CHUNKS && includePlayerChunks;
    }

    private void refresh() {
        progress = totalWeight == 0 ? 0.0F : (doneWeight + currentFraction * currentWeight) / totalWeight;
    }

    public float get() {
        return progress;
    }

    public void updateFocus(ResourceKey<Level> dimension, ChunkPos chunkPos) {}
}
