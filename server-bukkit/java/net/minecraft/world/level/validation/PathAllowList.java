// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.validation;
import java.io.BufferedReader;
import java.nio.file.FileSystem;
import java.nio.file.Path;
import java.nio.file.PathMatcher;
import java.util.List;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import java.util.regex.Pattern;
import java.util.Map;
public class PathAllowList implements PathMatcher {
    private static final org.slf4j.Logger kLog = org.slf4j.LoggerFactory.getLogger(PathAllowList.class);
    private final List<ConfigEntry> entries;
    private final Map<String, PathMatcher> compiled = new ConcurrentHashMap<>();
    public PathAllowList(List<ConfigEntry> entries) {
        this.entries = entries;
    }
    public PathMatcher getForFileSystem(FileSystem fileSystem) {
        return compiled.computeIfAbsent(fileSystem.provider().getScheme(), scheme -> {
            List<PathMatcher> matchers;
            try {
                matchers = entries.stream().map(entry -> entry.compile(fileSystem)).toList();
            } catch (Exception failure) {
                kLog.error("Failed to compile file pattern list", failure);
                return path -> false;
            }
            return path -> matchers.stream().anyMatch(matcher -> matcher.matches(path));
        });
    }
    @Override
    public boolean matches(Path path) {
        return getForFileSystem(path.getFileSystem()).matches(path);
    }
    public static PathAllowList readPlain(BufferedReader reader) {
        return new PathAllowList(reader.lines().flatMap(line -> ConfigEntry.parse(line).stream()).toList());
    }
    @FunctionalInterface
    public interface EntryType {
        EntryType FILESYSTEM = FileSystem::getPathMatcher;
        EntryType PREFIX = (fileSystem, pattern) -> path -> path.toString().startsWith(pattern);
        PathMatcher compile(FileSystem fileSystem, String pattern);
    }
    public static record ConfigEntry(EntryType type, String pattern) {
        public PathMatcher compile(FileSystem fileSystem) {
            return type.compile(fileSystem, pattern);
        }
        public static Optional<ConfigEntry> parse(String definition) {
            if (definition.isBlank() || definition.startsWith("#")) {
                return Optional.empty();
            }
            if (!definition.startsWith("[")) {
                return Optional.of(prefix(definition));
            }
            int close = definition.indexOf(']', 1);
            if (close == -1) {
                throw new IllegalArgumentException("Unterminated type in line '" + definition + "'");
            }
            String kind = definition.substring(1, close);
            String rest = definition.substring(close + 1);
            return switch (kind) {
                case "glob" -> Optional.of(glob(rest));
                case "regex" -> Optional.of(regex(rest));
                case "prefix" -> Optional.of(prefix(rest));
                default -> throw new IllegalArgumentException("Unsupported definition type in line '" + definition + "'");
            };
        }
        static ConfigEntry glob(String pattern) {
            return new ConfigEntry(EntryType.FILESYSTEM, "glob:" + pattern);
        }
        static ConfigEntry regex(String pattern) {
            return new ConfigEntry(EntryType.FILESYSTEM, "regex:" + pattern);
        }
        static ConfigEntry prefix(String pattern) {
            return new ConfigEntry(EntryType.PREFIX, pattern);
        }
    }
}
