// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.storage;

import net.minecraft.ReportedException;
import net.minecraft.world.level.ChunkPos;
public interface ChunkIOErrorReporter {
    default void reportChunkLoadFailure(Throwable throwable, RegionStorageInfo storageInfo, ChunkPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.ChunkIOErrorReporter#reportChunkLoadFailure");
    }
    default void reportChunkSaveFailure(Throwable throwable, RegionStorageInfo storageInfo, ChunkPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.ChunkIOErrorReporter#reportChunkSaveFailure");
    }
    static ReportedException createMisplacedChunkReport(ChunkPos storedPos, ChunkPos requestedPos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.ChunkIOErrorReporter#createMisplacedChunkReport");
    }
    default void reportMisplacedChunk(ChunkPos storedPos, ChunkPos requestedPos, RegionStorageInfo storageInfo) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.ChunkIOErrorReporter#reportMisplacedChunk");
    }
}
