// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.linkfs;
import java.nio.file.*;
import java.nio.file.attribute.*;
import java.io.*;
import java.net.URI;
import java.util.*;
import net.minecraft.util.DummyFileAttributes;
import java.io.File;
import java.io.IOException;
import java.net.URI;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.WatchEvent;
import java.nio.file.WatchKey;
import java.nio.file.WatchService;
import java.nio.file.attribute.BasicFileAttributeView;
import java.nio.file.attribute.BasicFileAttributes;
class LinkFSPath implements Path {
    private final LinkFileSystem fileSystem;
    private final String name;
    private final LinkFSPath parent;
    private final PathContents contents;

    private List<String> names() {
        if (name.isEmpty()) {
            return List.of();
        }
        List<String> chain = new ArrayList<>(parent == null ? List.of() : parent.names());
        chain.add(name);
        return chain;
    }

    private LinkFSPath linkOf(Path path) {
        if (path instanceof LinkFSPath link && link.fileSystem == fileSystem) {
            return link;
        }
        throw path == null ? new NullPointerException() : new ProviderMismatchException();
    }

    public LinkFSPath(LinkFileSystem fileSystem, String name, LinkFSPath parent, PathContents pathContents) {
        this.fileSystem = fileSystem;
        this.name = name;
        this.parent = parent;
        contents = pathContents;
        
    }
    public LinkFileSystem getFileSystem() {
        return fileSystem;
    }
    public boolean isAbsolute() {
        return contents != PathContents.RELATIVE;
    }
    public File toFile() {
        if (contents instanceof PathContents.FileContents file) {
            return file.contents().toFile();
        }
        throw new UnsupportedOperationException("Path " + toString() + " does not represent file");
    }
    public LinkFSPath getRoot() {
        return isAbsolute() ? fileSystem.rootPath() : null;
    }
    public LinkFSPath getFileName() {
        return new LinkFSPath(fileSystem, name, null, PathContents.RELATIVE);
    }
    public LinkFSPath getParent() {
        return parent;
    }
    public int getNameCount() {
        return names().size();
    }
    public LinkFSPath getName(int index) {
        List<String> all = names();
        if (index < 0 || index >= all.size()) {
            throw new IllegalArgumentException("No name at index " + index);
        }
        return new LinkFSPath(fileSystem, all.get(index), null, PathContents.RELATIVE);
    }
    public LinkFSPath subpath(int beginIndex, int endIndex) {
        List<String> all = names();
        if (beginIndex < 0 || endIndex > all.size() || beginIndex >= endIndex) {
            throw new IllegalArgumentException("bad range");
        }
        LinkFSPath chain = null;
        for (int index = beginIndex; index < endIndex; index++) {
            chain = new LinkFSPath(fileSystem, all.get(index), chain, PathContents.RELATIVE);
        }
        return chain;
    }
    public boolean startsWith(Path other) {
        if (!(other instanceof LinkFSPath that) || that.fileSystem != fileSystem || that.isAbsolute() != isAbsolute()) {
            return false;
        }
        List<String> mine = names();
        List<String> theirs = that.names();
        return theirs.size() <= mine.size() && mine.subList(0, theirs.size()).equals(theirs);
    }
    public boolean endsWith(Path other) {
        if (!(other instanceof LinkFSPath that) || that.fileSystem != fileSystem || (that.isAbsolute() && !isAbsolute())) {
            return false;
        }
        List<String> mine = names();
        List<String> theirs = that.names();
        return theirs.size() <= mine.size() && mine.subList(mine.size() - theirs.size(), mine.size()).equals(theirs);
    }
    public LinkFSPath normalize() {
        return this;
    }
    public LinkFSPath resolve(Path other) {
        LinkFSPath that = linkOf(other);
        if (that.isAbsolute()) {
            return that;
        }
        LinkFSPath current = this;
        for (String segment : that.names()) {
            current = current.resolveName(segment);
        }
        return current;
    }
    LinkFSPath resolveName(String name) {
        if (contents instanceof PathContents.DirectoryContents directory) {
            LinkFSPath child = directory.children().get(name);
            return child != null ? child : new LinkFSPath(fileSystem, name, this, PathContents.MISSING);
        }
        if (contents instanceof PathContents.FileContents) {
            return new LinkFSPath(fileSystem, name, this, PathContents.MISSING);
        }
        return new LinkFSPath(fileSystem, name, this, contents);
    }
    public LinkFSPath relativize(Path other) {
        LinkFSPath that = linkOf(other);
        if (isAbsolute() != that.isAbsolute()) {
            throw new IllegalArgumentException("absolute path mismatch");
        }
        List<String> mine = names();
        List<String> theirs = that.names();
        if (mine.size() >= theirs.size() || !theirs.subList(0, mine.size()).equals(mine)) {
            throw new IllegalArgumentException("bad range");
        }
        return that.subpath(mine.size(), theirs.size());
    }
    public URI toUri() {
        try {
            return new URI(LinkFSProvider.SCHEME, fileSystem.store().name(), toString(), null);
        } catch (java.net.URISyntaxException failure) {
            throw new AssertionError("Failed to create URI", failure);
        }
    }
    public LinkFSPath toAbsolutePath() {
        return isAbsolute() ? this : fileSystem.rootPath().resolve(this);
    }
    public LinkFSPath toRealPath(LinkOption... options) {
        return toAbsolutePath();
    }
    public WatchKey register(WatchService watcher, WatchEvent.Kind<?>[] events, WatchEvent.Modifier... modifiers) {
        throw new UnsupportedOperationException("links are read-only");
    }
    public int compareTo(Path other) {
        return toString().compareTo(linkOf(other).toString());
    }
    public boolean equals(Object other) {
        if (other == this) {
            return true;
        }
        if (!(other instanceof LinkFSPath that) || that.fileSystem != fileSystem || exists() != that.exists()) {
            return false;
        }
        return exists() ? contents == that.contents : Objects.equals(parent, that.parent) && Objects.equals(name, that.name);
    }
    public int hashCode() {
        return exists() ? contents.hashCode() : name.hashCode();
    }
    public String toString() {
        return (isAbsolute() ? "/" : "") + String.join("/", names());
    }
    public boolean exists() {
        return contents != PathContents.MISSING && contents != PathContents.RELATIVE;
    }
    public Path getTargetPath() {
        return contents instanceof PathContents.FileContents file ? file.contents() : null;
    }
    public PathContents. DirectoryContents getDirectoryContents() {
        return contents instanceof PathContents.DirectoryContents directory ? directory : null;
    }
    public BasicFileAttributeView getBasicAttributeView() {
        BasicFileAttributeView view = new BasicFileAttributeView() {
            @Override
            public String name() {
                return "basic";
            }

            @Override
            public BasicFileAttributes readAttributes() throws IOException {
                return getBasicAttributes();
            }

            @Override
            public void setTimes(FileTime modified, FileTime accessed, FileTime created) {
                throw new ReadOnlyFileSystemException();
            }
        };
        return view;
    }
    public BasicFileAttributes getBasicAttributes() throws IOException {
        if (contents instanceof PathContents.DirectoryContents) {
            return DummyFileAttributes.DIRECTORY;
        }
        if (contents instanceof PathContents.FileContents) {
            return DummyFileAttributes.FILE;
        }
        throw new NoSuchFileException(toString());
    }
}
