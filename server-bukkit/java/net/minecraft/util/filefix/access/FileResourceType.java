// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.access;

import java.nio.file.Path;

public class FileResourceType<T extends AutoCloseable> {
    private final AccessFactory<T> factory;

    public FileResourceType(AccessFactory<T> factory) {
        this.factory = factory;
    }

    public T create(Path path, int dataVersion) {
        return factory.create(path, dataVersion);
    }

    @FunctionalInterface
    public interface AccessFactory<T> {
        T create(Path path, int dataVersion);
    }
}
