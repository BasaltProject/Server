// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.access;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.stream.Stream;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

@FunctionalInterface
public interface FileRelation {
    Logger LOGGER = LoggerFactory.getLogger(FileRelation.class);
    FileRelation ORIGIN = List::of;
    FileRelation REGION = base -> List.of(base.resolve("region"));
    FileRelation DATA = base -> List.of(base.resolve("data"));
    FileRelation PLAYER_DATA = base -> List.of(base.resolve("players/data"));
    FileRelation DIMENSIONS = FileRelation::discoverDimensions;
    FileRelation DIMENSIONS_DATA = DIMENSIONS.resolve(DATA);
    FileRelation GENERATED_NAMESPACES = ORIGIN.resolve(base -> directoriesIn(base.resolve("generated")));
    FileRelation OLD_OVERWORLD = ORIGIN;
    FileRelation OLD_NETHER = ORIGIN.resolve(base -> List.of(base.resolve("DIM-1")));
    FileRelation OLD_END = ORIGIN.resolve(base -> List.of(base.resolve("DIM1")));

    List<Path> getPaths(Path basePath);

    static FileRelation forDataFileInDimension(String dimension, String fileName) {
        return base -> List.of(base.resolve("dimensions/minecraft/" + dimension + "/data/" + fileName));
    }

    default FileRelation forFile(String fileName) {
        return resolve(base -> List.of(base.resolve(fileName)));
    }

    default FileRelation resolve(FileRelation other) {
        return base -> getPaths(base).stream().flatMap(path -> other.getPaths(path).stream()).toList();
    }

    default FileRelation join(FileRelation... relations) {
        return base -> {
            Set<Path> paths = new LinkedHashSet<>();
            for (FileRelation relation : relations) {
                paths.addAll(relation.getPaths(base));
            }
            return List.copyOf(paths);
        };
    }

    private static List<Path> discoverDimensions(Path base) {
        Path root = base.resolve("dimensions");
        if (!Files.exists(root)) {
            return defaultDimensions(base);
        }
        List<Path> found = new ArrayList<>();
        for (Path namespace : directoriesIn(root)) {
            found.addAll(directoriesIn(namespace));
        }
        return found.isEmpty() ? defaultDimensions(base) : found;
    }

    static List<Path> directoriesIn(Path path) {
        try (Stream<Path> children = Files.list(path)) {
            return children.filter(Files::isDirectory).toList();
        } catch (IOException failure) {
            return List.of();
        }
    }

    private static List<Path> defaultDimensions(Path base) {
        return List.of(base.resolve("dimensions/minecraft/overworld"), base.resolve("dimensions/minecraft/the_nether"),
            base.resolve("dimensions/minecraft/the_end"));
    }
}
