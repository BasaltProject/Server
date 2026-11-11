// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import net.minecraft.world.level.chunk.status.ChunkPyramid;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.chunk.status.ChunkStep;

public class ChunkLevel {
    private static final int FULL_LEVEL = 33;
    private static final int BLOCK_TICKING_LEVEL = FULL_LEVEL - 1;
    private static final int ENTITY_TICKING_LEVEL = FULL_LEVEL - 2;
    private static final ChunkStep FULL_STEP = ChunkPyramid.GENERATION_PYRAMID.getStepTo(ChunkStatus.FULL);
    public static final int RADIUS_AROUND_FULL_CHUNK = FULL_STEP.accumulatedDependencies().getRadius();
    public static final int MAX_LEVEL = FULL_LEVEL + RADIUS_AROUND_FULL_CHUNK;

    public static ChunkStatus generationStatus(int level) {
        return getStatusAroundFullChunk(level - FULL_LEVEL, null);
    }

    public static ChunkStatus getStatusAroundFullChunk(int distanceToFullChunk, ChunkStatus defaultValue) {
        if (distanceToFullChunk > RADIUS_AROUND_FULL_CHUNK) {
            return defaultValue;
        }
        return distanceToFullChunk > 0 ? FULL_STEP.accumulatedDependencies().get(distanceToFullChunk) : ChunkStatus.FULL;
    }

    public static ChunkStatus getStatusAroundFullChunk(int distanceToFullChunk) {
        return getStatusAroundFullChunk(distanceToFullChunk, ChunkStatus.EMPTY);
    }

    public static int byStatus(ChunkStatus status) {
        return FULL_LEVEL + FULL_STEP.getAccumulatedRadiusOf(status);
    }

    public static FullChunkStatus fullStatus(int level) {
        if (level <= ENTITY_TICKING_LEVEL) {
            return FullChunkStatus.ENTITY_TICKING;
        }
        if (level <= BLOCK_TICKING_LEVEL) {
            return FullChunkStatus.BLOCK_TICKING;
        }
        return level <= FULL_LEVEL ? FullChunkStatus.FULL : FullChunkStatus.INACCESSIBLE;
    }

    public static int byStatus(FullChunkStatus status) {
        return switch (status) {
            case INACCESSIBLE -> MAX_LEVEL;
            case FULL -> FULL_LEVEL;
            case BLOCK_TICKING -> BLOCK_TICKING_LEVEL;
            case ENTITY_TICKING -> ENTITY_TICKING_LEVEL;
        };
    }

    public static boolean isEntityTicking(int level) {
        return level <= ENTITY_TICKING_LEVEL;
    }

    public static boolean isBlockTicking(int level) {
        return level <= BLOCK_TICKING_LEVEL;
    }

    public static boolean isLoaded(int level) {
        return level <= MAX_LEVEL;
    }
}
