// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
import com.mojang.serialization.DataResult;
import java.io.File;
import java.io.IOException;
import java.nio.file.FileAlreadyExistsException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
public class FileUtil {
    private static final Pattern kCopyCounter = Pattern.compile("(?<name>.*) \\((?<count>\\d*)\\)", Pattern.CASE_INSENSITIVE | Pattern.UNICODE_CASE);
    private static final int kMaxFileName = 255;
    private static final Pattern kReservedWindowsNames = Pattern.compile(".*\\.|(?:COM|CLOCK\\$|CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\..*)?", Pattern.CASE_INSENSITIVE);
    private static final Pattern kStrictSegment = Pattern.compile("[-._a-z0-9]+");
    private static final char[] kIllegalFileCharacters = {'/', '\n', '\r', '\t', '\0', '\f', '`', '?', '*', '\\', '<', '>', '|', '"', ':'};
    public static String sanitizeName(String baseName) {
        for (char illegal : kIllegalFileCharacters) {
            baseName = baseName.replace(illegal, '_');
        }
        return baseName.replaceAll("[./\"]", "_");
    }
    public static String findAvailableName(Path baseDir, String baseName, String suffix) throws IOException {
        baseName = sanitizeName(baseName);
        if (!isPathPartPortable(baseName)) {
            baseName = "_" + baseName + "_";
        }
        Matcher matcher = kCopyCounter.matcher(baseName);
        int count = 0;
        if (matcher.matches()) {
            baseName = matcher.group("name");
            count = Integer.parseInt(matcher.group("count"));
        }
        if (baseName.length() > kMaxFileName - suffix.length()) {
            baseName = baseName.substring(0, kMaxFileName - suffix.length());
        }
        while (true) {
            String candidate = baseName;
            if (count != 0) {
                String counter = " (" + count + ")";
                int room = kMaxFileName - counter.length();
                candidate = (baseName.length() > room ? baseName.substring(0, room) : baseName) + counter;
            }
            Path full = baseDir.resolve(candidate + suffix);
            try {
                Path created = Files.createDirectory(full);
                Files.deleteIfExists(created);
                return baseDir.relativize(created).toString();
            } catch (FileAlreadyExistsException taken) {
                count++;
            }
        }
    }
    public static boolean isPathPortable(Path path) {
        for (Path part : path) {
            if (!isPathPartPortable(part.toString())) {
                return false;
            }
        }
        return true;
    }
    public static boolean isPathPartPortable(String name) {
        return !kReservedWindowsNames.matcher(name).matches();
    }
    public static String getFullResourcePath(String filename) {
        int cut = Math.max(filename.lastIndexOf('/'), filename.lastIndexOf('\\'));
        return cut < 0 ? "" : filename.substring(0, cut + 1).replace(File.separator, "/");
    }
    public static String normalizeResourcePath(String filename) {
        String normalized = java.nio.file.Paths.get(filename).normalize().toString();
        return normalized.replace(File.separator, "/");
    }
    public static DataResult<List<String>> decomposePath(String path) {
        List<String> segments = new ArrayList<>(Arrays.asList(path.split("/", -1)));
        for (String segment : segments) {
            if (segment.isEmpty() || segment.equals(".") || segment.equals("..") || !kStrictSegment.matcher(segment).matches()) {
                return DataResult.error(() -> segments.size() == 1 ? "Invalid path '" + path + "'" : "Invalid segment '" + segment + "' in path '" + path + "'");
            }
        }
        return DataResult.success(segments);
    }
    public static Path resolvePath(Path root, List<String> segments) {
        Path resolved = root;
        for (String segment : segments) {
            resolved = resolved.resolve(segment);
        }
        return resolved;
    }
    public static boolean isValidPathSegment(String segment) {
        return !segment.equals("..") && !segment.equals(".") && kStrictSegment.matcher(segment).matches();
    }
    public static void validatePath(String... path) {
        if (path.length == 0) {
            throw new IllegalArgumentException("Path must have at least one element");
        }
        for (String segment : path) {
            if (!isValidPathSegment(segment)) {
                throw new IllegalArgumentException("Illegal segment " + segment + " in path " + Arrays.toString(path));
            }
        }
    }
    public static void createDirectoriesSafe(Path dir) throws IOException {
        Files.createDirectories(Files.exists(dir) ? dir.toRealPath() : dir);
    }
    public static boolean isEmptyPath(Path path) {
        return path.getNameCount() == 1 && path.getFileName().toString().isEmpty();
    }
}
