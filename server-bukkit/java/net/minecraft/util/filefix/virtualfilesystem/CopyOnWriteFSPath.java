// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.virtualfilesystem;

import java.io.IOException;
import java.net.URI;
import java.nio.file.FileSystem;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.ProviderMismatchException;
import java.nio.file.WatchEvent;
import java.nio.file.WatchKey;
import java.nio.file.WatchService;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

public final class CopyOnWriteFSPath implements Path {
    private final CopyOnWriteFileSystem fileSystem;
    private final boolean absolute;
    private final List<String> names;

    private CopyOnWriteFSPath(CopyOnWriteFileSystem fileSystem, boolean absolute, List<String> names) {
        this.fileSystem = fileSystem;
        this.absolute = absolute;
        this.names = List.copyOf(names);
    }

    static CopyOnWriteFSPath of(CopyOnWriteFileSystem fileSystem, String first, String... more) {
        StringBuilder joined = new StringBuilder(first);
        for (String part : more) {
            if (!part.isEmpty()) {
                joined.append('/').append(part);
            }
        }
        String text = joined.toString();
        List<String> names = new ArrayList<>();
        for (String name : text.split("/")) {
            if (!name.isEmpty()) {
                names.add(name);
            }
        }
        return new CopyOnWriteFSPath(fileSystem, text.startsWith("/"), names);
    }

    public static CopyOnWriteFSPath asCow(Path path) {
        if (path instanceof CopyOnWriteFSPath cow) {
            return cow;
        }
        throw new ProviderMismatchException("Not a copy-on-write path: " + path);
    }

    public boolean isRoot() {
        return absolute && names.isEmpty();
    }

    @Override
    public CopyOnWriteFileSystem getFileSystem() {
        return fileSystem;
    }

    @Override
    public boolean isAbsolute() {
        return absolute;
    }

    @Override
    public CopyOnWriteFSPath getRoot() {
        return absolute ? fileSystem.rootPath() : null;
    }

    @Override
    public CopyOnWriteFSPath getFileName() {
        return names.isEmpty() ? null : new CopyOnWriteFSPath(fileSystem, false, List.of(names.getLast()));
    }

    @Override
    public CopyOnWriteFSPath getParent() {
        if (names.isEmpty()) {
            return null;
        }
        if (names.size() == 1) {
            return absolute ? fileSystem.rootPath() : null;
        }
        return new CopyOnWriteFSPath(fileSystem, absolute, names.subList(0, names.size() - 1));
    }

    @Override
    public int getNameCount() {
        return names.size();
    }

    @Override
    public CopyOnWriteFSPath getName(int index) {
        if (index < 0 || index >= names.size()) {
            throw new IllegalArgumentException("No name at index " + index);
        }
        return new CopyOnWriteFSPath(fileSystem, false, List.of(names.get(index)));
    }

    @Override
    public CopyOnWriteFSPath subpath(int beginIndex, int endIndex) {
        if (beginIndex < 0 || endIndex > names.size() || beginIndex >= endIndex) {
            throw new IllegalArgumentException("Bad subpath range " + beginIndex + ".." + endIndex);
        }
        return new CopyOnWriteFSPath(fileSystem, false, names.subList(beginIndex, endIndex));
    }

    @Override
    public boolean startsWith(Path other) {
        if (!(other instanceof CopyOnWriteFSPath cow) || cow.absolute != absolute || cow.names.size() > names.size()) {
            return false;
        }
        return names.subList(0, cow.names.size()).equals(cow.names);
    }

    @Override
    public boolean endsWith(Path other) {
        if (!(other instanceof CopyOnWriteFSPath cow) || cow.names.size() > names.size()) {
            return false;
        }
        if (cow.absolute && (!absolute || cow.names.size() != names.size())) {
            return false;
        }
        return names.subList(names.size() - cow.names.size(), names.size()).equals(cow.names);
    }

    @Override
    public CopyOnWriteFSPath normalize() {
        List<String> kept = new ArrayList<>();
        for (String name : names) {
            if (name.equals(".")) {
                continue;
            }
            if (name.equals("..")) {
                if (!kept.isEmpty() && !kept.getLast().equals("..")) {
                    kept.removeLast();
                    continue;
                }
                if (absolute) {
                    continue;
                }
            }
            kept.add(name);
        }
        return new CopyOnWriteFSPath(fileSystem, absolute, kept);
    }

    @Override
    public CopyOnWriteFSPath resolve(Path other) {
        CopyOnWriteFSPath cow = asCow(other);
        if (cow.absolute) {
            return cow;
        }
        List<String> joined = new ArrayList<>(names);
        joined.addAll(cow.names);
        return new CopyOnWriteFSPath(fileSystem, absolute, joined);
    }

    @Override
    public CopyOnWriteFSPath resolve(String other) {
        return resolve(fileSystem.getPath(other));
    }

    @Override
    public CopyOnWriteFSPath resolveSibling(Path other) {
        CopyOnWriteFSPath parent = getParent();
        return parent == null ? asCow(other) : parent.resolve(other);
    }

    @Override
    public CopyOnWriteFSPath resolveSibling(String other) {
        return resolveSibling(fileSystem.getPath(other));
    }

    @Override
    public CopyOnWriteFSPath relativize(Path other) {
        CopyOnWriteFSPath cow = asCow(other);
        if (cow.absolute != absolute) {
            throw new IllegalArgumentException("Cannot relativize an absolute path against a relative one");
        }
        int shared = 0;
        while (shared < names.size() && shared < cow.names.size() && names.get(shared).equals(cow.names.get(shared))) {
            shared++;
        }
        List<String> result = new ArrayList<>();
        for (int index = shared; index < names.size(); index++) {
            result.add("..");
        }
        result.addAll(cow.names.subList(shared, cow.names.size()));
        return new CopyOnWriteFSPath(fileSystem, false, result);
    }

    @Override
    public URI toUri() {
        return URI.create(CopyOnWriteFSProvider.SCHEME + ":" + toAbsolutePath());
    }

    @Override
    public CopyOnWriteFSPath toAbsolutePath() {
        return absolute ? this : new CopyOnWriteFSPath(fileSystem, true, names);
    }

    @Override
    public CopyOnWriteFSPath toRealPath(LinkOption... options) throws IOException {
        return fileSystem.provider().getRealPath(this);
    }

    @Override
    public WatchKey register(WatchService watcher, WatchEvent.Kind<?>[] events, WatchEvent.Modifier... modifiers) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int compareTo(Path other) {
        return toString().compareTo(other.toString());
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof CopyOnWriteFSPath cow && cow.fileSystem == fileSystem && cow.absolute == absolute
            && cow.names.equals(names);
    }

    @Override
    public int hashCode() {
        return Objects.hash(absolute, names);
    }

    @Override
    public String toString() {
        return (absolute ? "/" : "") + String.join("/", names);
    }
}
