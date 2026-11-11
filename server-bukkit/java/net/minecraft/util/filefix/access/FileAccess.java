// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.access;

import java.nio.file.Path;
import java.util.List;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class FileAccess<T extends AutoCloseable> implements AutoCloseable {
    private static final Logger LOGGER = LoggerFactory.getLogger(FileAccess.class);
    private final FileAccessProvider provider;
    private final FileResourceType<T> type;
    private final FileRelation relation;
    private List<T> files;

    public FileAccess(FileAccessProvider provider, FileResourceType<T> type, FileRelation relation) {
        this.provider = provider;
        this.type = type;
        this.relation = relation;
    }

    public List<T> get() {
        if (files == null) {
            Path base = provider.baseDirectory().orElse(null);
            if (base == null) {
                throw new IllegalStateException("Cannot access world files");
            }
            files = relation.getPaths(base).stream().map(path -> type.create(path, provider.dataVersion())).toList();
        }
        return files;
    }

    public T getOnlyFile() {
        List<T> all = get();
        if (all.size() != 1) {
            throw new IllegalStateException("Trying to get only file, but there are " + all.size() + " files");
        }
        return all.getFirst();
    }

    @Override
    public void close() {
        if (files == null) {
            return;
        }
        for (T file : files) {
            try {
                file.close();
            } catch (Exception failure) {
                LOGGER.error("Failed to close file: ", failure);
            }
        }
        files = null;
    }
}
