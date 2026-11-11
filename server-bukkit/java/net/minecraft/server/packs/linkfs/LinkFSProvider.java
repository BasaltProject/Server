// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.linkfs;
import java.nio.file.*;
import java.nio.file.attribute.*;
import java.nio.file.spi.*;
import java.nio.channels.*;
import java.io.*;
import java.net.URI;
import java.util.*;
import java.util.Iterator;
import java.util.List;
import java.io.IOException;
import java.net.URI;
import java.nio.channels.SeekableByteChannel;
import java.nio.file.AccessMode;
import java.nio.file.CopyOption;
import java.nio.file.DirectoryStream;
import java.nio.file.FileStore;
import java.nio.file.FileSystem;
import java.nio.file.LinkOption;
import java.nio.file.OpenOption;
import java.nio.file.Path;
import java.nio.file.attribute.BasicFileAttributes;
import java.nio.file.attribute.FileAttribute;
import java.nio.file.attribute.FileAttributeView;
import java.nio.file.spi.FileSystemProvider;
import java.util.Map;
import java.util.Set;
final class LinkFSProvider extends FileSystemProvider {
    private static ReadOnlyFileSystemException readOnly() {
        return new ReadOnlyFileSystemException();
    }

    private static UnsupportedOperationException unsupported() {
        return new UnsupportedOperationException();
    }

    private static NoSuchFileException missing(Path path) {
        return new NoSuchFileException(path.toString());
    }

    private static LinkFSPath asLink(Path path) {
        if (path == null) {
            throw new NullPointerException();
        }
        if (path instanceof LinkFSPath link) {
            return link;
        }
        throw new ProviderMismatchException("not a link path");
    }

    public static final String SCHEME = "x-mc-link";
    public String getScheme() {
        return SCHEME;
    }
    public FileSystem newFileSystem(URI uri, Map<String, ?> env) {
        throw unsupported();
    }
    public FileSystem getFileSystem(URI uri) {
        throw unsupported();
    }
    public Path getPath(URI uri) {
        throw unsupported();
    }
    public SeekableByteChannel newByteChannel(Path path, Set<? extends OpenOption> options, FileAttribute<?>... attrs) throws IOException {
        if (options.stream().anyMatch(option -> option == StandardOpenOption.CREATE || option == StandardOpenOption.CREATE_NEW
            || option == StandardOpenOption.APPEND || option == StandardOpenOption.WRITE)) {
            throw unsupported();
        }
        Path target = asLink(path).toAbsolutePath().getTargetPath();
        if (target == null) {
            throw missing(path);
        }
        return Files.newByteChannel(target, options, attrs);
    }
    public DirectoryStream<Path> newDirectoryStream(Path dir, DirectoryStream.Filter<? super Path> filter) throws IOException {
        PathContents.DirectoryContents contents = asLink(dir).toAbsolutePath().getDirectoryContents();
        if (contents == null) {
            throw new NotDirectoryException(String.valueOf(dir));
        }
        return new DirectoryStream<>() {
            @Override
            public Iterator<Path> iterator() {
                List<Path> accepted = new java.util.ArrayList<>();
                for (LinkFSPath child : contents.children().values()) {
                    try {
                        if (filter.accept(child)) {
                            accepted.add(child);
                        }
                    } catch (IOException failure) {
                        throw new DirectoryIteratorException(failure);
                    }
                }
                return accepted.iterator();
            }

            @Override
            public void close() {
            }
        };
    }
    public void createDirectory(Path dir, FileAttribute<?>... attrs) {
        throw readOnly();
    }
    public void delete(Path path) {
        throw readOnly();
    }
    public void copy(Path source, Path target, CopyOption... options) {
        throw readOnly();
    }
    public void move(Path source, Path target, CopyOption... options) {
        throw readOnly();
    }
    public boolean isSameFile(Path path, Path path2) {
        return path instanceof LinkFSPath && path.equals(path2);
    }
    public boolean isHidden(Path path) {
        return false;
    }
    public FileStore getFileStore(Path path) {
        return asLink(path).getFileSystem().store();
    }
    public void checkAccess(Path path, AccessMode... modes) throws IOException {
        LinkFSPath link = asLink(path);
        for (AccessMode mode : modes) {
            if (mode != AccessMode.READ) {
                throw new AccessDeniedException(String.valueOf(mode));
            }
        }
        if (!link.exists()) {
            throw missing(path);
        }
    }
    public <V extends FileAttributeView> V getFileAttributeView(Path path, Class<V> type, LinkOption... options) {
        LinkFSPath link = asLink(path);
        return type == BasicFileAttributeView.class ? type.cast(link.getBasicAttributeView()) : null;
    }
    public <A extends BasicFileAttributes> A readAttributes(Path path, Class<A> type, LinkOption... options) throws IOException {
        if (type != BasicFileAttributes.class) {
            throw new UnsupportedOperationException(type.getName() + " attributes are not supported");
        }
        return type.cast(asLink(path).toAbsolutePath().getBasicAttributes());
    }
    public Map<String, Object> readAttributes(Path path, String attributes, LinkOption... options) {
        throw unsupported();
    }
    public void setAttribute(Path path, String attribute, Object value, LinkOption... options) {
        throw readOnly();
    }
}
