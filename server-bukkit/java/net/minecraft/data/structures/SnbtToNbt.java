// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.structures;

import com.google.common.hash.Hashing;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.stream.Stream;
import net.minecraft.data.CachedOutput;
import net.minecraft.data.DataProvider;
import net.minecraft.data.PackOutput;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtIo;
import net.minecraft.nbt.NbtUtils;

public class SnbtToNbt implements DataProvider {
    private final PackOutput output;
    private final Iterable<Path> inputFolders;
    private final List<Filter> filters = new ArrayList<>();

    public SnbtToNbt(PackOutput output, Path inputFolder) {
        this(output, List.of(inputFolder));
    }

    public SnbtToNbt(PackOutput output, Iterable<Path> inputFolders) {
        this.output = output;
        this.inputFolders = inputFolders;
    }

    public SnbtToNbt addFilter(Filter filter) {
        filters.add(filter);
        return this;
    }

    @Override
    public CompletableFuture<?> run(CachedOutput cache) {
        return CompletableFuture.runAsync(() -> {
            for (Path folder : inputFolders) {
                try (Stream<Path> files = Files.walk(folder)) {
                    for (Path file : (Iterable<Path>) files.filter(p -> p.toString().endsWith(".snbt"))::iterator) {
                        String name = folder.relativize(file).toString().replace('\\', '/');
                        name = name.substring(0, name.length() - ".snbt".length());
                        convert(cache, file, name);
                    }
                } catch (IOException e) {
                    throw new StructureConversionException(folder, e);
                }
            }
        });
    }

    private void convert(CachedOutput cache, Path file, String name) {
        try {
            CompoundTag tag = NbtUtils.snbtToStructure(Files.readString(file, StandardCharsets.UTF_8));
            for (Filter filter : filters) {
                tag = filter.apply(name, tag);
            }
            ByteArrayOutputStream bytes = new ByteArrayOutputStream();
            NbtIo.writeCompressed(tag, bytes);
            byte[] payload = bytes.toByteArray();
            Path target = output.getOutputFolder().resolve(name + ".nbt");
            cache.writeIfNeeded(target, payload, Hashing.sha1().hashBytes(payload));
        } catch (Exception e) {
            throw new StructureConversionException(file, e);
        }
    }

    @Override
    public final String getName() {
        return "SNBT -> NBT";
    }

    static record TaskResult(String name, byte[] payload, com.google.common.hash.HashCode hash) {
    }

    static class StructureConversionException extends RuntimeException {
        public StructureConversionException(Path path, Throwable t) {
            super(path.toAbsolutePath().toString(), t);
        }
    }

    public interface Filter {
        CompoundTag apply(String name, CompoundTag input);
    }
}
