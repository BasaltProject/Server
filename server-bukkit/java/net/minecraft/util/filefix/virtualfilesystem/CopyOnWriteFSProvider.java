// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.virtualfilesystem;

import java.io.IOException;
import java.net.URI;
import java.nio.channels.FileChannel;
import java.nio.channels.SeekableByteChannel;
import java.nio.file.AccessMode;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.CopyOption;
import java.nio.file.DirectoryStream;
import java.nio.file.FileStore;
import java.nio.file.FileSystem;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.OpenOption;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.nio.file.StandardOpenOption;
import java.nio.file.attribute.BasicFileAttributeView;
import java.nio.file.attribute.BasicFileAttributes;
import java.nio.file.attribute.FileAttribute;
import java.nio.file.attribute.FileAttributeView;
import java.nio.file.attribute.FileTime;
import java.nio.file.spi.FileSystemProvider;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import net.minecraft.util.DummyFileAttributes;
import net.minecraft.util.filefix.virtualfilesystem.exception.CowFSDirectoryNotEmptyException;
import net.minecraft.util.filefix.virtualfilesystem.exception.CowFSFileAlreadyExistsException;
import net.minecraft.util.filefix.virtualfilesystem.exception.CowFSFileSystemException;
import net.minecraft.util.filefix.virtualfilesystem.exception.CowFSNoSuchFileException;

public class CopyOnWriteFSProvider extends FileSystemProvider {
    public static final String SCHEME = "x-mc-copy-on-write";
    private static final BasicFileAttributeView DIRECTORY_VIEW = new BasicFileAttributeView() {
        @Override
        public String name() {
            return "basic";
        }

        @Override
        public BasicFileAttributes readAttributes() {
            return DummyFileAttributes.DIRECTORY;
        }

        @Override
        public void setTimes(FileTime modified, FileTime accessed, FileTime created) {}
    };
    private final CopyOnWriteFileSystem fs;

    public CopyOnWriteFSProvider(CopyOnWriteFileSystem fileSystem) {
        this.fs = fileSystem;
    }

    @Override
    public String getScheme() {
        return SCHEME;
    }

    @Override
    public FileSystem newFileSystem(URI uri, Map<String, ?> env) {
        throw new UnsupportedOperationException("CowFS is created directly");
    }

    @Override
    public FileSystem getFileSystem(URI uri) {
        throw new UnsupportedOperationException("CowFS has no URI lookup");
    }

    @Override
    public Path getPath(URI uri) {
        throw new UnsupportedOperationException("CowFS has no URI paths");
    }

    @Override
    public SeekableByteChannel newByteChannel(Path path, Set<? extends OpenOption> options, FileAttribute<?>... attrs)
            throws IOException {
        return openChannel(path, options, attrs, Files::newByteChannel);
    }

    @Override
    public FileChannel newFileChannel(Path path, Set<? extends OpenOption> options, FileAttribute<?>... attrs)
            throws IOException {
        return openChannel(path, options, attrs, FileChannel::open);
    }

    private synchronized <C> C openChannel(Path path, Set<? extends OpenOption> options, FileAttribute<?>[] attrs,
            ChannelFactory<C> factory) throws IOException {
        CopyOnWriteFSPath cow = CopyOnWriteFSPath.asCow(path);
        if (options.contains(StandardOpenOption.DELETE_ON_CLOSE)) {
            throw new UnsupportedOperationException("CowFS cannot delete a file on close");
        }
        Node existing = fs.fileTree().byPathOrNull(cow);
        if (existing == null) {
            boolean mayCreate = options.contains(StandardOpenOption.CREATE) || options.contains(StandardOpenOption.CREATE_NEW);
            if (!mayCreate) {
                throw new CowFSNoSuchFileException(cow.toString());
            }
            DirectoryNode parent = fs.fileTree().directoryByPath(cow.getParent());
            Path temporary = fs.createTemporaryFilePath();
            C channel = factory.open(temporary, options, attrs);
            parent.addChild(new FileNode(cow, temporary, true));
            return channel;
        }
        if (existing instanceof FileNode file) {
            if (options.contains(StandardOpenOption.CREATE_NEW)) {
                throw new CowFSFileAlreadyExistsException(cow.toString());
            }
            if (wantsWrite(options)) {
                file.ensureCopy();
            }
            return factory.open(file.storagePath(), options, attrs);
        }
        throw new CowFSFileSystemException(cow + ": not a regular file");
    }

    private static boolean wantsWrite(Set<? extends OpenOption> options) {
        return options.contains(StandardOpenOption.WRITE)
            || !options.contains(StandardOpenOption.READ) && options.contains(StandardOpenOption.APPEND);
    }

    @Override
    public synchronized DirectoryStream<Path> newDirectoryStream(Path dir, DirectoryStream.Filter<? super Path> filter)
            throws IOException {
        DirectoryNode directory = fs.fileTree().directoryByPath(CopyOnWriteFSPath.asCow(dir));
        List<Path> accepted = new ArrayList<>();
        for (Node child : directory.children()) {
            if (filter.accept(child.path())) {
                accepted.add(child.path());
            }
        }
        return new DirectoryStream<>() {
            @Override
            public void close() {}

            @Override
            public Iterator<Path> iterator() {
                return accepted.iterator();
            }
        };
    }

    @Override
    public synchronized void createDirectory(Path dir, FileAttribute<?>... attrs) throws IOException {
        CopyOnWriteFSPath cow = CopyOnWriteFSPath.asCow(dir);
        CopyOnWriteFSPath parent = cow.getParent();
        if (parent == null) {
            throw new CowFSFileAlreadyExistsException(cow.toString());
        }
        DirectoryNode parentDirectory = fs.fileTree().directoryByPath(parent);
        if (parentDirectory.getChild(cow.getFileName().toString()) != null) {
            throw new CowFSFileAlreadyExistsException(cow.toString());
        }
        parentDirectory.addChild(new DirectoryNode(cow));
    }

    @Override
    public synchronized void delete(Path path) throws IOException {
        CopyOnWriteFSPath cow = CopyOnWriteFSPath.asCow(path);
        Node node = fs.fileTree().byPath(cow);
        if (node.parent == null) {
            throw new CowFSFileSystemException("The root cannot be removed");
        }
        if (node instanceof DirectoryNode directory && !directory.children().isEmpty()) {
            throw new CowFSDirectoryNotEmptyException(cow.toString());
        }
        if (node instanceof FileNode file) {
            file.deleteCopy();
        }
        node.parent.removeChild(node.name());
    }

    @Override
    public synchronized void copy(Path source, Path target, CopyOption... options) throws IOException {
        CopyOnWriteFSPath from = CopyOnWriteFSPath.asCow(source);
        CopyOnWriteFSPath to = CopyOnWriteFSPath.asCow(target).toAbsolutePath();
        boolean replace = false;
        for (CopyOption option : options) {
            replace |= option == StandardCopyOption.REPLACE_EXISTING;
        }
        FileNode file = fs.fileTree().fileByPath(from);
        CopyOnWriteFSPath parentPath = to.getParent();
        if (parentPath == null || !(fs.fileTree().byPathOrNull(parentPath) instanceof DirectoryNode parent)) {
            throw new CowFSNoSuchFileException(to.toString());
        }
        String name = to.getFileName().toString();
        if (parent.getChild(name) != null) {
            if (!replace) {
                throw new CowFSFileAlreadyExistsException(to.toString());
            }
            if (parent.getChild(name) instanceof DirectoryNode) {
                throw new CowFSFileSystemException(to + ": cannot replace a directory");
            }
            delete(to);
        }
        Path duplicate = fs.createTemporaryFilePath();
        if (!org.basaltmc.bukkit.FileNatives.copy(file.storagePath().toString(), duplicate.toString())) {
            throw new IOException("Failed to copy " + file.storagePath());
        }
        parent.addChild(new FileNode(to, duplicate, true));
    }

    @Override
    public synchronized void move(Path source, Path target, CopyOption... options) throws IOException {
        CopyOnWriteFSPath from = CopyOnWriteFSPath.asCow(source);
        CopyOnWriteFSPath to = CopyOnWriteFSPath.asCow(target);
        if (from.isRoot()) {
            throw new CowFSFileSystemException(from + ": can't move root directory");
        }
        boolean replace = false;
        for (CopyOption option : options) {
            if (StandardCopyOption.ATOMIC_MOVE.equals(option)) {
                throw new AtomicMoveNotSupportedException(from.toString(), to.toString(), "CowFs does not support atomic move");
            }
            replace |= option.equals(StandardCopyOption.REPLACE_EXISTING);
        }
        Node moving = fs.fileTree().byPathOrNull(from);
        if (moving == null) {
            throw new CowFSNoSuchFileException(from.toString());
        }
        CopyOnWriteFSPath parentPath = to.toAbsolutePath().getParent();
        if (parentPath == null) {
            throw new CowFSFileAlreadyExistsException(to.toString());
        }
        if (!(fs.fileTree().byPathOrNull(parentPath) instanceof DirectoryNode targetParent)) {
            throw new CowFSNoSuchFileException(to.toString());
        }
        String name = to.getFileName().toString();
        Node displaced = targetParent.getChild(name);
        if (displaced != null) {
            if (displaced == moving) {
                return;
            }
            if (!replace) {
                throw new CowFSFileAlreadyExistsException(to.toString());
            }
            targetParent.removeChild(name);
        }
        moving.parent.removeChild(moving.name());
        moving.setPath(to);
        targetParent.addChild(moving);
    }

    @Override
    public boolean isSameFile(Path a, Path b) {
        return CopyOnWriteFSPath.asCow(a).toAbsolutePath().normalize()
            .equals(CopyOnWriteFSPath.asCow(b).toAbsolutePath().normalize());
    }

    @Override
    public boolean isHidden(Path path) {
        CopyOnWriteFSPath name = CopyOnWriteFSPath.asCow(path).getFileName();
        return name != null && name.toString().startsWith(".");
    }

    @Override
    public FileStore getFileStore(Path path) {
        return fs.store();
    }

    @Override
    public synchronized void checkAccess(Path path, AccessMode... modes) throws IOException {
        Path backing = switch (fs.fileTree().byPath(CopyOnWriteFSPath.asCow(path))) {
            case DirectoryNode directory -> fs.tmpDirectory();
            case FileNode file -> file.storagePath();
        };
        backing.getFileSystem().provider().checkAccess(backing, modes);
    }

    @Override
    @SuppressWarnings("unchecked")
    public synchronized <V extends FileAttributeView> V getFileAttributeView(Path path, Class<V> type,
            LinkOption... options) {
        CopyOnWriteFSPath cow = CopyOnWriteFSPath.asCow(path);
        Node node = fs.fileTree().byPathOrNull(cow);
        if (node == null) {
            return type == BasicFileAttributeView.class ? (V) missingView(cow) : null;
        }
        if (node instanceof FileNode file) {
            return Files.getFileAttributeView(file.storagePath(), type, options);
        }
        return type == BasicFileAttributeView.class ? (V) DIRECTORY_VIEW : null;
    }

    private static BasicFileAttributeView missingView(CopyOnWriteFSPath path) {
        return new BasicFileAttributeView() {
            @Override
            public String name() {
                return "basic";
            }

            @Override
            public BasicFileAttributes readAttributes() throws IOException {
                throw new CowFSNoSuchFileException(path.toString());
            }

            @Override
            public void setTimes(FileTime modified, FileTime accessed, FileTime created) throws IOException {
                throw new CowFSNoSuchFileException(path.toString());
            }
        };
    }

    @Override
    public synchronized <A extends BasicFileAttributes> A readAttributes(Path path, Class<A> type,
            LinkOption... options) throws IOException {
        return switch (fs.fileTree().byPath(CopyOnWriteFSPath.asCow(path))) {
            case DirectoryNode directory -> type.cast(DummyFileAttributes.DIRECTORY);
            case FileNode file -> Files.readAttributes(file.storagePath(), type, options);
        };
    }

    @Override
    public synchronized Map<String, Object> readAttributes(Path path, String attributes, LinkOption... options)
            throws IOException {
        return switch (fs.fileTree().byPath(CopyOnWriteFSPath.asCow(path))) {
            case DirectoryNode directory -> Map.of("isDirectory", true, "isRegularFile", false, "size", 0L);
            case FileNode file -> Files.readAttributes(file.storagePath(), attributes, options);
        };
    }

    @Override
    public void setAttribute(Path path, String attribute, Object value, LinkOption... options) {
        throw new UnsupportedOperationException("CowFS attributes are read only");
    }

    public synchronized CopyOnWriteFSPath getRealPath(CopyOnWriteFSPath path) throws CowFSNoSuchFileException {
        return fs.fileTree().byPath(path.toAbsolutePath()).path;
    }

    @FunctionalInterface
    private interface ChannelFactory<C> {
        C open(Path path, Set<? extends OpenOption> options, FileAttribute<?>... attrs) throws IOException;
    }
}
