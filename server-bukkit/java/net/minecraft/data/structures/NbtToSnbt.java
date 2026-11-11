// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.structures;

import com.google.common.hash.Hashing;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import java.util.stream.Stream;
import net.minecraft.data.CachedOutput;
import net.minecraft.data.DataProvider;
import net.minecraft.data.PackOutput;
import net.minecraft.nbt.NbtIo;
import net.minecraft.nbt.NbtUtils;

public class NbtToSnbt implements DataProvider {
    private final PackOutput output;
    private final Collection<Path> inputFolders;

    public NbtToSnbt(PackOutput output, Collection<Path> inputFolders) {
        this.output = output;
        this.inputFolders = inputFolders;
    }

    @Override
    public CompletableFuture<?> run(CachedOutput cache) {
        return CompletableFuture.runAsync(() -> {
            for (Path folder : inputFolders) {
                try (Stream<Path> files = Files.walk(folder)) {
                    for (Path file : (Iterable<Path>) files.filter(p -> p.toString().endsWith(".nbt"))::iterator) {
                        String name = folder.relativize(file).toString().replace('\\', '/');
                        convertStructure(cache, file, name.substring(0, name.length() - ".nbt".length()), output.getOutputFolder());
                    }
                } catch (IOException e) {
                    throw new IllegalStateException("Couldn't read " + folder, e);
                }
            }
        });
    }

    @Override
    public final String getName() {
        return "NBT -> SNBT";
    }

    public static Path convertStructure(CachedOutput cache, Path path, String name, Path output) {
        try {
            String text = NbtUtils.structureToSnbt(NbtIo.readCompressed(path, net.minecraft.nbt.NbtAccounter.unlimitedHeap()));
            Path destination = output.resolve(name + ".snbt");
            writeSnbt(cache, destination, text);
            return destination;
        } catch (IOException e) {
            throw new IllegalStateException("Couldn't convert " + path, e);
        }
    }

    public static void writeSnbt(CachedOutput cache, Path destination, String text) throws IOException {
        byte[] bytes = (text.replace("\r", "") + "\n").getBytes(StandardCharsets.UTF_8);
        cache.writeIfNeeded(destination, bytes, Hashing.sha1().hashBytes(bytes));
    }
}
