// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.io.Closeable;
import java.io.File;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import java.nio.file.FileSystem;
import java.nio.file.FileSystems;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Map;
import java.util.stream.Stream;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class FileZipper implements Closeable {
    private static final Logger kLogger = LoggerFactory.getLogger(FileZipper.class);
    private final Path outputFile;
    private final Path tempFile;
    private final FileSystem fs;

    public FileZipper(Path outputFile) {
        this.outputFile = outputFile;
        this.tempFile = outputFile.resolveSibling(outputFile.getFileName() + "_tmp");
        try {
            this.fs = FileSystems.newFileSystem(URI.create("jar:" + tempFile.toUri()), Map.of("create", "true"));
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }

    private Path entry(Path relative) throws IOException {
        Path target = fs.getPath(File.separator).resolve(relative.toString());
        if (target.getParent() != null) {
            Files.createDirectories(target.getParent());
        }
        return target;
    }

    public void add(Path destinationRelativePath, String content) {
        try {
            Files.write(entry(destinationRelativePath), content.getBytes(StandardCharsets.UTF_8));
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }

    public void add(Path destinationRelativePath, File file) {
        try {
            Files.copy(file.toPath(), entry(destinationRelativePath));
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }

    public void add(Path path) {
        try {
            if (Files.isRegularFile(path)) {
                Files.copy(path, entry(path.getFileName()));
                return;
            }
            List<Path> sources;
            try (Stream<Path> found = Files.find(path, Integer.MAX_VALUE, (candidate, attributes) -> attributes.isRegularFile())) {
                sources = found.toList();
            }
            for (Path source : sources) {
                Files.copy(source, entry(path.relativize(source)));
            }
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }

    @Override
    public void close() {
        try {
            fs.close();
            Files.move(tempFile, outputFile);
            kLogger.info("Compressed to {}", outputFile);
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }
}
