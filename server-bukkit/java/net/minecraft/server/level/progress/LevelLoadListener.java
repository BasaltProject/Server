// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level.progress;

import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Level;

public interface LevelLoadListener {
    static LevelLoadListener compose(LevelLoadListener first, LevelLoadListener second) {
        return new LevelLoadListener() {
            public void start(Stage stage, int totalChunks) {
                first.start(stage, totalChunks);
                second.start(stage, totalChunks);
            }

            public void update(Stage stage, int currentChunks, int totalChunks) {
                first.update(stage, currentChunks, totalChunks);
                second.update(stage, currentChunks, totalChunks);
            }

            public void finish(Stage stage) {
                first.finish(stage);
                second.finish(stage);
            }

            public void updateFocus(ResourceKey<Level> dimension, ChunkPos chunkPos) {
                first.updateFocus(dimension, chunkPos);
                second.updateFocus(dimension, chunkPos);
            }
        };
    }

    void start(Stage stage, int totalChunks);

    void update(Stage stage, int currentChunks, int totalChunks);

    void finish(Stage stage);

    void updateFocus(ResourceKey<Level> dimension, ChunkPos chunkPos);

    public static enum Stage {
        START_SERVER, PREPARE_GLOBAL_SPAWN, LOAD_INITIAL_CHUNKS, LOAD_PLAYER_CHUNKS;
    }
}
