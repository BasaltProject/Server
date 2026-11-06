// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.storage;

import net.minecraft.CrashReport;
import net.minecraft.ReportedException;
import net.minecraft.world.level.ChunkPos;
public interface ChunkIOErrorReporter {
    default void reportChunkLoadFailure(Throwable throwable, RegionStorageInfo storageInfo, ChunkPos pos) {
        org.slf4j.LoggerFactory.getLogger("ChunkStorage").error("Failed to load chunk {} of {}", pos, storageInfo.type(), throwable);
    }
    default void reportChunkSaveFailure(Throwable throwable, RegionStorageInfo storageInfo, ChunkPos pos) {
        org.slf4j.LoggerFactory.getLogger("ChunkStorage").error("Failed to save chunk {} of {}", pos, storageInfo.type(), throwable);
    }
    static ReportedException createMisplacedChunkReport(ChunkPos storedPos, ChunkPos requestedPos) {
        IllegalStateException cause = new IllegalStateException("Retrieved chunk position " + storedPos + " does not match requested " + requestedPos);
        return new ReportedException(CrashReport.forThrowable(cause, "Chunk found in the wrong position"));
    }
    default void reportMisplacedChunk(ChunkPos storedPos, ChunkPos requestedPos, RegionStorageInfo storageInfo) {
        org.slf4j.LoggerFactory.getLogger("ChunkStorage").error("Chunk {} of {} was stored at {}", requestedPos, storageInfo.type(), storedPos);
    }
}
