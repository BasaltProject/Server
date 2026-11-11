// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.linkfs;
import java.nio.file.*;
import java.nio.file.attribute.*;
import java.nio.file.spi.*;
import java.nio.channels.*;
import java.io.*;
import java.net.URI;
import java.util.*;
import java.io.IOException;
import java.nio.file.FileStore;
import java.nio.file.attribute.FileAttributeView;
import java.nio.file.attribute.FileStoreAttributeView;
class LinkFSFileStore extends FileStore {
    private final String label;

    public LinkFSFileStore(String name) {
        label = name;
        
    }
    public String name() {
        return label;
    }
    public String type() {
        return "index";
    }
    public boolean isReadOnly() {
        return true;
    }
    public long getTotalSpace() {
        return 0L;
    }
    public long getUsableSpace() {
        return 0L;
    }
    public long getUnallocatedSpace() {
        return 0L;
    }
    public boolean supportsFileAttributeView(Class<? extends FileAttributeView> type) {
        return type == BasicFileAttributeView.class;
    }
    public boolean supportsFileAttributeView(String name) {
        return "basic".equals(name);
    }
    public <V extends FileStoreAttributeView> V getFileStoreAttributeView(Class<V> type) {
        return null;
    }
    public Object getAttribute(String attribute) throws IOException {
        throw new UnsupportedOperationException();
    }
}
