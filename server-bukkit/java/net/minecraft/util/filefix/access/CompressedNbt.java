// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.access;

import com.mojang.serialization.Dynamic;
import java.io.IOException;
import java.nio.file.NoSuchFileException;
import java.nio.file.Path;
import java.util.Optional;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtAccounter;
import net.minecraft.nbt.NbtIo;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.util.FileUtil;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public abstract class CompressedNbt implements AutoCloseable {
    private static final Logger LOGGER = LoggerFactory.getLogger(CompressedNbt.class);
    private final Path path;
    private final MissingSeverity missingSeverity;

    public CompressedNbt(Path path, MissingSeverity missingSeverity) {
        this.path = path;
        this.missingSeverity = missingSeverity;
    }

    public abstract Optional<Dynamic<Tag>> read() throws IOException;

    protected final Optional<Dynamic<Tag>> readFile() throws IOException {
        try {
            return Optional.of(new Dynamic<Tag>(NbtOps.INSTANCE, NbtIo.readCompressed(path, NbtAccounter.unlimitedHeap())));
        } catch (NoSuchFileException missing) {
            missingSeverity.log("Missing file: {}", path);
            return Optional.empty();
        }
    }

    public abstract <T> void write(Dynamic<T> data);

    protected final <T> void writeFile(Dynamic<T> data) {
        CompoundTag tag = (CompoundTag) data.convert(NbtOps.INSTANCE).getValue();
        try {
            FileUtil.createDirectoriesSafe(path.getParent());
            NbtIo.writeCompressed(tag, path);
        } catch (IOException failure) {
            LOGGER.error("Failed to write to {}", path, failure);
        }
    }

    public Path path() {
        return path;
    }

    @Override
    public void close() {}

    public enum MissingSeverity {
        IMPORTANT, NEUTRAL, MINOR;

        public void log(String message, Path path) {
            switch (this) {
                case IMPORTANT -> LOGGER.error(message, path);
                case NEUTRAL -> LOGGER.info(message, path);
                case MINOR -> LOGGER.debug(message, path);
            }
        }
    }
}
