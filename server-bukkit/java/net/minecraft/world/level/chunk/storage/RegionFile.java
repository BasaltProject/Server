// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.storage;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.file.Path;
import net.minecraft.world.level.ChunkPos;
import org.basaltmc.bukkit.WorldNatives;
public class RegionFile implements AutoCloseable {
    protected static final int SECTOR_INTS = 1024;
    protected final RegionBitmap usedSectors = new RegionBitmap();
    private final RegionStorageInfo info;
    private final Path path;
    private final RegionFileVersion version;
    private boolean closed;
    public RegionFile(RegionStorageInfo info, Path path, Path externalFileDir, boolean sync) throws IOException {
        this(info, path, externalFileDir, RegionFileVersion.getSelected(), sync);
    }
    public RegionFile(RegionStorageInfo info, Path path, Path externalFileDir, RegionFileVersion version, boolean sync) throws IOException {
        this.info = info;
        this.path = path;
        this.version = version;
    }
    public Path getPath() {
        return path;
    }
    public DataInputStream getChunkDataInputStream(ChunkPos pos) throws IOException {
        byte[] stored = WorldNatives.storedChunk(info.level(), pos.x(), pos.z());
        return stored == null ? null : new DataInputStream(new ByteArrayInputStream(stored));
    }
    public boolean doesChunkExist(ChunkPos pos) {
        return hasChunk(pos);
    }
    public DataOutputStream getChunkDataOutputStream(ChunkPos pos) throws IOException {
        return new DataOutputStream(new ChunkBuffer(pos));
    }
    public void flush() throws IOException {
    }
    public void clear(ChunkPos pos) throws IOException {
        WorldNatives.storeChunk(info.level(), pos.x(), pos.z(), new byte[0]);
    }
    protected void write(ChunkPos pos, ByteBuffer data) throws IOException {
        byte[] bytes = new byte[data.remaining()];
        data.get(bytes);
        if (!WorldNatives.storeChunk(info.level(), pos.x(), pos.z(), bytes)) {
            throw new IOException("Chunk " + pos + " could not be written");
        }
    }
    public boolean hasChunk(ChunkPos pos) {
        return !closed && WorldNatives.hasStoredChunk(info.level(), pos.x(), pos.z());
    }
    public void close() throws IOException {
        closed = true;
    }
    class ChunkBuffer extends ByteArrayOutputStream {
        private final ChunkPos pos;
        public ChunkBuffer(ChunkPos pos) {
            super(8096);
            this.pos = pos;
        }
        public void close() throws IOException {
            RegionFile.this.write(pos, ByteBuffer.wrap(buf, 0, count));
        }
    }
    interface CommitOp {
        void run() throws IOException;
    }
}
