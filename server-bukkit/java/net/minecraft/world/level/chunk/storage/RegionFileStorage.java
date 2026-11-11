// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.storage;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.nio.file.Path;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtAccounter;
import net.minecraft.nbt.NbtIo;
import net.minecraft.nbt.StreamTagVisitor;
import net.minecraft.world.level.ChunkPos;
public final class RegionFileStorage implements AutoCloseable {
    public static final String ANVIL_EXTENSION = ".mca";
    private final RegionStorageInfo info;
    private final Path folder;
    private final boolean sync;
    private RegionFile file;
    public RegionFileStorage(RegionStorageInfo info, Path folder, boolean sync) {
        this.info = info;
        this.folder = folder;
        this.sync = sync;
    }
    private RegionFile file() throws IOException {
        if (file == null) {
            file = new RegionFile(info, folder.resolve("r.0.0" + ANVIL_EXTENSION), folder, sync);
        }
        return file;
    }
    public CompoundTag read(ChunkPos pos) throws IOException {
        try (DataInputStream in = file().getChunkDataInputStream(pos)) {
            return in == null ? null : NbtIo.read(in);
        }
    }
    public void scanChunk(ChunkPos pos, StreamTagVisitor scanner) throws IOException {
        try (DataInputStream in = file().getChunkDataInputStream(pos)) {
            if (in != null) {
                NbtIo.parse(in, scanner, NbtAccounter.unlimitedHeap());
            }
        }
    }
    public void write(ChunkPos pos, CompoundTag value) throws IOException {
        if (value == null) {
            file().clear(pos);
            return;
        }
        try (DataOutputStream out = file().getChunkDataOutputStream(pos)) {
            NbtIo.write(value, out);
        }
    }
    public void close() throws IOException {
        if (file != null) {
            file.close();
        }
    }
    public void flush() throws IOException {
        file().flush();
    }
    public RegionStorageInfo info() {
        return info;
    }
}
