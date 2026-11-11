// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix;

import java.io.IOException;
import java.nio.file.FileAlreadyExistsException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.List;
import java.util.Locale;
import java.util.stream.Stream;
import net.minecraft.util.FileUtil;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class FileFixUtil {
    private static final Logger LOGGER = LoggerFactory.getLogger(FileFixUtil.class);
    public static final String NAMESPACE_PATTERN = "([a-z0-9_.-]+)";

    public static void moveFile(Path baseDirectory, String from, String to) throws IOException {
        Path source = baseDirectory.resolve(from);
        if (!Files.exists(source)) {
            return;
        }
        Path target = baseDirectory.resolve(to);
        if (Files.exists(target)) {
            throw new FileAlreadyExistsException(
                String.format(Locale.ROOT, "Cannot move file from %s to %s: Target already exists", from, to));
        }
        FileUtil.createDirectoriesSafe(target.getParent());
        Files.move(source, target, StandardCopyOption.COPY_ATTRIBUTES);
    }

    public static void deleteFileOrEmptyDirectory(Path baseDirectory, String file) throws IOException {
        Path target = baseDirectory.resolve(file);
        if (!Files.exists(target)) {
            return;
        }
        if (Files.isDirectory(target)) {
            try {
                List<Path> content = list(target);
                if (content.size() == 1 && content.getFirst().getFileName().toString().equals(".DS_Store")
                    && !Files.deleteIfExists(content.getFirst())) {
                    LOGGER.warn("Failed to delete file '{}' at '{}'", content.getFirst(), target);
                }
                content = list(target);
                if (!content.isEmpty()) {
                    LOGGER.warn("Failed to delete directory '{}', as it's not empty. Content: {}", target, content);
                    return;
                }
            } catch (IOException failure) {
                LOGGER.warn("Failed to delete directory '{}' because {}", target, failure.toString());
                return;
            }
        }
        Files.deleteIfExists(target);
    }

    private static List<Path> list(Path directory) throws IOException {
        try (Stream<Path> paths = Files.list(directory)) {
            return paths.toList();
        }
    }
}
