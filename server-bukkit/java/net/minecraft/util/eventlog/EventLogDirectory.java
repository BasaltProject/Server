// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.eventlog;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.Reader;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.channels.FileLock;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.time.LocalDate;
import java.time.format.DateTimeFormatter;
import java.time.format.DateTimeParseException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;
import java.util.Set;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class EventLogDirectory {
    private static final Logger kLogger = LoggerFactory.getLogger(EventLogDirectory.class);
    private static final String kCompressedExtension = ".gz";
    private final Path root;
    private final String extension;

    private EventLogDirectory(Path root, String extension) {
        this.root = root;
        this.extension = extension;
    }

    public static EventLogDirectory open(Path root, String extension) throws IOException {
        Files.createDirectories(root);
        return new EventLogDirectory(root, extension);
    }

    public FileList listFiles() throws IOException {
        try (Stream<Path> entries = Files.list(root)) {
            return new FileList(entries.filter(Files::isRegularFile).map(this::classify).filter(f -> f != null).toList());
        }
    }

    private File classify(Path path) {
        String name = path.getFileName().toString();
        int dot = name.indexOf('.');
        if (dot == -1) {
            return null;
        }
        FileId id = FileId.parse(name.substring(0, dot));
        if (id == null) {
            return null;
        }
        String suffix = name.substring(dot);
        if (suffix.equals(extension)) {
            return new RawFile(path, id);
        }
        return suffix.equals(extension + kCompressedExtension) ? new CompressedFile(path, id) : null;
    }

    private static void compress(Path raw, Path target) throws IOException {
        if (Files.exists(target)) {
            throw new IOException("Compressed target file already exists: " + target);
        }
        try (FileChannel channel = FileChannel.open(raw, StandardOpenOption.WRITE, StandardOpenOption.READ)) {
            FileLock lock = channel.tryLock();
            if (lock == null) {
                throw new IOException("Raw log file is already locked, cannot compress: " + raw);
            }
            try (OutputStream out = new GZIPOutputStream(Files.newOutputStream(target))) {
                byte[] bytes = new byte[4096];
                ByteBuffer buffer = ByteBuffer.wrap(bytes);
                while (channel.read(buffer) >= 0) {
                    buffer.flip();
                    out.write(bytes, 0, buffer.limit());
                    buffer.clear();
                }
            }
            channel.truncate(0L);
        }
        Files.delete(raw);
    }

    public RawFile createNewFile(LocalDate date) throws IOException {
        Set<FileId> taken = listFiles().ids();
        int index = 1;
        FileId id = new FileId(date, index);
        while (taken.contains(id)) {
            id = new FileId(date, ++index);
        }
        RawFile file = new RawFile(root.resolve(id.toFileName(extension)), id);
        Files.createFile(file.path());
        return file;
    }

    public static class FileList implements Iterable<File> {
        private final List<File> files;

        private FileList(List<File> files) {
            this.files = new ArrayList<>(files);
        }

        public FileList prune(LocalDate date, int expiryDays) {
            files.removeIf(file -> {
                if (date.isBefore(file.id().date().plusDays(expiryDays))) {
                    return false;
                }
                try {
                    Files.delete(file.path());
                    return true;
                } catch (IOException e) {
                    kLogger.warn("Failed to delete expired event log file: {}", file.path(), e);
                    return false;
                }
            });
            return this;
        }

        public FileList compressAll() {
            ListIterator<File> it = files.listIterator();
            while (it.hasNext()) {
                File file = it.next();
                try {
                    it.set(file.compress());
                } catch (IOException e) {
                    kLogger.warn("Failed to compress event log file: {}", file.path(), e);
                }
            }
            return this;
        }

        @Override
        public Iterator<File> iterator() {
            return files.iterator();
        }

        public Stream<File> stream() {
            return files.stream();
        }

        public Set<FileId> ids() {
            return files.stream().map(File::id).collect(Collectors.toSet());
        }
    }

    public static record RawFile(Path path, FileId id) implements File {
        public FileChannel openChannel() throws IOException {
            return FileChannel.open(path, StandardOpenOption.WRITE, StandardOpenOption.READ);
        }

        @Override
        public Reader openReader() throws IOException {
            return Files.exists(path) ? Files.newBufferedReader(path) : null;
        }

        @Override
        public CompressedFile compress() throws IOException {
            Path target = path.resolveSibling(path.getFileName() + kCompressedExtension);
            EventLogDirectory.compress(path, target);
            return new CompressedFile(target, id);
        }
    }

    public static record CompressedFile(Path path, FileId id) implements File {
        @Override
        public Reader openReader() throws IOException {
            if (!Files.exists(path)) {
                return null;
            }
            return new BufferedReader(new InputStreamReader(new GZIPInputStream(Files.newInputStream(path)), StandardCharsets.UTF_8));
        }

        @Override
        public CompressedFile compress() {
            return this;
        }
    }

    public static record FileId(LocalDate date, int index) {
        private static final DateTimeFormatter kDate = DateTimeFormatter.BASIC_ISO_DATE;

        public static FileId parse(String name) {
            int dash = name.indexOf('-');
            if (dash == -1) {
                return null;
            }
            try {
                return new FileId(LocalDate.parse(name.substring(0, dash), kDate), Integer.parseInt(name.substring(dash + 1)));
            } catch (DateTimeParseException | NumberFormatException e) {
                return null;
            }
        }

        @Override
        public String toString() {
            return kDate.format(date) + "-" + index;
        }

        public String toFileName(String extension) {
            return this + extension;
        }
    }

    public interface File {
        Path path();

        FileId id();

        Reader openReader() throws IOException;

        CompressedFile compress() throws IOException;
    }
}
