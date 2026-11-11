// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.virtualfilesystem;

import java.io.IOException;
import java.nio.file.FileStore;
import java.nio.file.Files;
import java.nio.file.attribute.BasicFileAttributeView;
import java.nio.file.attribute.FileAttributeView;
import java.nio.file.attribute.FileStoreAttributeView;

public class CopyOnWriteFileStore extends FileStore {
    private final String name;
    private final CopyOnWriteFileSystem fileSystem;

    public CopyOnWriteFileStore(String name, CopyOnWriteFileSystem fileSystem) {
        this.name = name;
        this.fileSystem = fileSystem;
    }

    @Override
    public String name() {
        return name;
    }

    @Override
    public String type() {
        return "copy-on-write";
    }

    @Override
    public boolean isReadOnly() {
        return false;
    }

    private FileStore backing() throws IOException {
        return Files.getFileStore(fileSystem.tmpDirectory());
    }

    @Override
    public long getTotalSpace() throws IOException {
        return backing().getTotalSpace();
    }

    @Override
    public long getUsableSpace() throws IOException {
        return backing().getUsableSpace();
    }

    @Override
    public long getUnallocatedSpace() throws IOException {
        return backing().getUnallocatedSpace();
    }

    @Override
    public boolean supportsFileAttributeView(Class<? extends FileAttributeView> type) {
        return type == BasicFileAttributeView.class;
    }

    @Override
    public boolean supportsFileAttributeView(String name) {
        return name.equals("basic");
    }

    @Override
    public <V extends FileStoreAttributeView> V getFileStoreAttributeView(Class<V> type) {
        return null;
    }

    @Override
    public Object getAttribute(String attribute) {
        throw new UnsupportedOperationException();
    }
}
