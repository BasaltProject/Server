// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.nio.file.*;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.*;
import java.nio.file.Path;
import java.nio.file.attribute.FileTime;
import java.util.Comparator;
public class DownloadCacheCleaner {
    private static List<PathAndTime> collect(Path cacheDir) throws IOException {
        List<PathAndTime> found = new ArrayList<>();
        try {
            Files.walkFileTree(cacheDir, new SimpleFileVisitor<Path>() {
                @Override
                public FileVisitResult visitFile(Path file, BasicFileAttributes attributes) {
                    if (attributes.isRegularFile() && !file.getParent().equals(cacheDir)) {
                        found.add(new PathAndTime(file, attributes.lastModifiedTime()));
                    }
                    return FileVisitResult.CONTINUE;
                }
            });
        } catch (NoSuchFileException absent) {
            return List.of();
        }
        return found;
    }

    public static void vacuumCacheDir(Path cacheDir, int maxFiles) {
        try {
            List<PathAndTime> files = collect(cacheDir);
            int excess = files.size() - maxFiles;
            if (excess <= 0) {
                return;
            }
            files.sort(PathAndTime.NEWEST_FIRST);
            Map<Path, Integer> perDirectory = new HashMap<>();
            List<PathAndPriority> ranked = new ArrayList<>();
            for (PathAndTime file : files) {
                ranked.add(new PathAndPriority(file.path(), perDirectory.merge(file.path().getParent(), 1, Integer::sum) - 1));
            }
            Collections.reverse(ranked);
            ranked.sort(PathAndPriority.HIGHEST_PRIORITY_FIRST);
            Set<Path> parents = new HashSet<>();
            for (PathAndPriority doomed : ranked.subList(0, excess)) {
                try {
                    Files.delete(doomed.path());
                    if (doomed.removalPriority() == 0) {
                        parents.add(doomed.path().getParent());
                    }
                } catch (IOException ignored) {
                    continue;
                }
            }
            parents.remove(cacheDir);
            for (Path directory : parents) {
                try {
                    Files.delete(directory);
                } catch (IOException ignored) {
                    continue;
                }
            }
        } catch (IOException | UncheckedIOException failure) {
            return;
        }
    }
    static record PathAndTime(Path path, FileTime modifiedTime) {
        public static final Comparator<PathAndTime> NEWEST_FIRST = Comparator.comparing(PathAndTime::modifiedTime).reversed();
    }
    static record PathAndPriority(Path path, int removalPriority) {
        public static final Comparator<PathAndPriority> HIGHEST_PRIORITY_FIRST = Comparator.comparing(PathAndPriority::removalPriority).reversed();
    }
}
