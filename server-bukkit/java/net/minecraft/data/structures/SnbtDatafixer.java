// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.structures;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.stream.Stream;
import net.minecraft.data.CachedOutput;
import net.minecraft.nbt.NbtUtils;

public class SnbtDatafixer {
    public static void main(String[] args) throws IOException {
        for (String folder : args) {
            try (Stream<Path> files = Files.walk(Path.of(folder))) {
                for (Path file : (Iterable<Path>) files.filter(p -> p.toString().endsWith(".snbt"))::iterator) {
                    String text = Files.readString(file, StandardCharsets.UTF_8);
                    try {
                        NbtToSnbt.writeSnbt(CachedOutput.NO_CACHE, file, NbtUtils.structureToSnbt(StructureUpdater.update(file.toString(), NbtUtils.snbtToStructure(text))));
                    } catch (com.mojang.brigadier.exceptions.CommandSyntaxException e) {
                        throw new IOException("Couldn't parse " + file, e);
                    }
                }
            }
        }
    }
}
