// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.access;

import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

public class FileAccessProvider implements AutoCloseable {
    private final List<FileAccess<?>> accessedFiles = new ArrayList<>();
    private final ScopedValue<Path> baseDirectory = ScopedValue.newInstance();
    private final int dataVersion;
    private boolean frozen;

    public FileAccessProvider(int dataVersion) {
        this.dataVersion = dataVersion;
    }

    public <T extends AutoCloseable> FileAccess<T> getFileAccess(FileResourceType<T> type, FileRelation relation) {
        if (frozen) {
            throw new IllegalStateException("Cannot request new file access here.");
        }
        FileAccess<T> access = new FileAccess<>(this, type, relation);
        accessedFiles.add(access);
        return access;
    }

    public void freeze() {
        frozen = true;
    }

    public ScopedValue<Path> baseDirectory() {
        return baseDirectory;
    }

    public int dataVersion() {
        return dataVersion;
    }

    @Override
    public void close() {
        for (FileAccess<?> access : accessedFiles) {
            access.close();
        }
    }
}
