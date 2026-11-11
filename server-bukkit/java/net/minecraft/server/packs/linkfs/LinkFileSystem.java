// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.linkfs;
import java.nio.file.*;
import java.nio.file.attribute.*;
import java.nio.file.spi.*;
import java.nio.channels.*;
import java.io.*;
import java.net.URI;
import java.util.*;
import java.nio.file.FileStore;
import java.nio.file.FileSystem;
import java.nio.file.Path;
import java.nio.file.PathMatcher;
import java.nio.file.WatchService;
import java.nio.file.attribute.UserPrincipalLookupService;
import java.nio.file.spi.FileSystemProvider;
import java.util.List;
import java.util.Map;
import java.util.Set;
public class LinkFileSystem extends FileSystem {
    public static final String PATH_SEPARATOR = "/";
    private final FileStore store;
    private final FileSystemProvider provider = new LinkFSProvider();
    private final LinkFSPath root;

    private static LinkFSPath materialize(DirectoryEntry entry, LinkFileSystem system, String name, LinkFSPath parent) {
        Map<String, LinkFSPath> kids = new java.util.HashMap<>();
        LinkFSPath directory = new LinkFSPath(system, name, parent, new PathContents.DirectoryContents(kids));
        entry.files().forEach((fileName, target) -> kids.put(fileName, new LinkFSPath(system, fileName, directory, new PathContents.FileContents(target))));
        entry.children().forEach((childName, child) -> kids.put(childName, materialize(child, system, childName, directory)));
        return directory;
    }

    private LinkFileSystem(String name, DirectoryEntry rootEntry) {
        store = new LinkFSFileStore(name);
        root = materialize(rootEntry, this, "", null);
        
    }
    public FileSystemProvider provider() {
        return provider;
    }
    public void close() {
        return;
    }
    public boolean isOpen() {
        return true;
    }
    public boolean isReadOnly() {
        return true;
    }
    public String getSeparator() {
        return PATH_SEPARATOR;
    }
    public Iterable<Path> getRootDirectories() {
        return List.of(root);
    }
    public Iterable<FileStore> getFileStores() {
        return List.of(store);
    }
    public Set<String> supportedFileAttributeViews() {
        return Set.of("basic");
    }
    public Path getPath(String first, String... more) {
        String joined = String.join(PATH_SEPARATOR, java.util.stream.Stream.concat(java.util.stream.Stream.of(first), java.util.Arrays.stream(more)).toList());
        if (joined.equals(PATH_SEPARATOR)) {
            return root;
        }
        boolean absolute = joined.startsWith(PATH_SEPARATOR);
        LinkFSPath current = absolute ? root : null;
        for (String segment : (absolute ? joined.substring(1) : joined).split(PATH_SEPARATOR, -1)) {
            if (segment.isEmpty()) {
                throw new IllegalArgumentException("Empty paths not allowed");
            }
            current = absolute ? current.resolveName(segment) : new LinkFSPath(this, segment, current, PathContents.RELATIVE);
        }
        return current;
    }
    public PathMatcher getPathMatcher(String syntaxAndPattern) {
        throw new UnsupportedOperationException();
    }
    public UserPrincipalLookupService getUserPrincipalLookupService() {
        throw new UnsupportedOperationException();
    }
    public WatchService newWatchService() {
        throw new UnsupportedOperationException();
    }
    public FileStore store() {
        return store;
    }
    public LinkFSPath rootPath() {
        return root;
    }
    public static Builder builder() {
        return new Builder();
    }
    static record DirectoryEntry(Map<String, DirectoryEntry> children, Map<String, Path> files) {
        public DirectoryEntry() {
            this(new java.util.HashMap<>(), new java.util.HashMap<>());
        }
    }
    public static class Builder {
        private final DirectoryEntry root = new DirectoryEntry();

        public Builder put(List<String> path, String name, Path target) {
            DirectoryEntry node = root;
            for (String segment : path) {
                node = node.children().computeIfAbsent(segment, key -> new DirectoryEntry());
            }
            node.files().put(name, target);
            return this;
        }
        public Builder put(List<String> path, Path target) {
            if (path.isEmpty()) {
                throw new IllegalArgumentException("Path can't be empty");
            }
            int last = path.size() - 1;
            return put(path.subList(0, last), path.get(last), target);
        }
        public FileSystem build(String name) {
            return new LinkFileSystem(name, root);
        }
    }
}
