// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.virtualfilesystem;

import java.io.IOException;
import java.nio.file.CopyOption;
import java.nio.file.FileStore;
import java.nio.file.FileSystem;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.PathMatcher;
import java.nio.file.WatchService;
import java.nio.file.attribute.UserPrincipalLookupService;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;
import net.minecraft.util.Util;
import net.minecraft.util.filefix.virtualfilesystem.exception.CowFSCreationException;
import net.minecraft.util.filefix.virtualfilesystem.exception.CowFSSymlinkException;
import org.basaltmc.bukkit.FileNatives;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class CopyOnWriteFileSystem extends FileSystem {
    private static final Set<String> ATTRIBUTE_VIEWS = Set.of("basic");
    private static final Logger LOGGER = LoggerFactory.getLogger(CopyOnWriteFileSystem.class);
    private final CopyOnWriteFileStore store;
    private final CopyOnWriteFSProvider provider;
    private final Path baseDirectory;
    private final PathMatcher skippedPaths;
    private final Path tmpDirectory;
    private final CopyOnWriteFSPath rootPath;
    private final AtomicInteger tmpFileIndex = new AtomicInteger();
    private DirectoryNode fileTree;

    private CopyOnWriteFileSystem(String name, Path baseDirectory, Path tmpDirectory, PathMatcher skippedPaths)
            throws IOException {
        this.baseDirectory = baseDirectory;
        this.tmpDirectory = tmpDirectory;
        this.skippedPaths = skippedPaths;
        this.store = new CopyOnWriteFileStore(name, this);
        this.provider = new CopyOnWriteFSProvider(this);
        this.rootPath = getPath("/");
        this.fileTree = buildFileTreeFrom(baseDirectory);
    }

    public static CopyOnWriteFileSystem create(String name, Path baseDirectory, Path tmpDirectory,
            PathMatcher skippedPaths) throws IOException {
        if (FileNatives.stat(tmpDirectory.toString()) != null) {
            throw new CowFSCreationException("Temporary directory already exists: " + tmpDirectory);
        }
        CopyOnWriteFileSystem created = new CopyOnWriteFileSystem(name, baseDirectory, tmpDirectory, skippedPaths);
        if (!FileNatives.makeDirectories(tmpDirectory.toString())) {
            throw new CowFSCreationException("Cannot create temporary directory: " + tmpDirectory);
        }
        return created;
    }

    private DirectoryNode buildFileTreeFrom(Path base) throws IOException {
        if (!Files.isWritable(base)) {
            throw new CowFSCreationException("Cannot build copy-on-write file system, missing write access for: " + base);
        }
        DirectoryNode tree = new DirectoryNode(rootPath);
        String skippedDirectory = null;
        for (String entry : FileNatives.walk(base.toString())) {
            char kind = entry.charAt(0);
            String relative = entry.substring(2);
            if (skippedDirectory != null && relative.startsWith(skippedDirectory)) {
                continue;
            }
            Path real = base.resolve(relative);
            if (kind == 'o') {
                throw new CowFSSymlinkException("Cannot build copy-on-write file system when symlink is present: " + real);
            }
            if (skippedPaths.matches(real)) {
                skippedDirectory = kind == 'd' ? relative + "/" : null;
                continue;
            }
            CopyOnWriteFSPath cowPath = tree.path().resolve(relative);
            DirectoryNode parent = tree.directoryByPath(Objects.requireNonNull(cowPath.getParent()));
            parent.addChild(kind == 'd' ? new DirectoryNode(cowPath) : new FileNode(cowPath, real, false));
        }
        return tree;
    }

    protected void resetFileTreeToBaseFolderContent() throws IOException {
        fileTree = buildFileTreeFrom(baseDirectory);
    }

    @Override
    public CopyOnWriteFSProvider provider() {
        return provider;
    }

    @Override
    public void close() {
        FileNatives.removeTree(tmpDirectory.toString());
    }

    @Override
    public boolean isOpen() {
        return true;
    }

    @Override
    public boolean isReadOnly() {
        return false;
    }

    @Override
    public String getSeparator() {
        return backingFileSystem().getSeparator();
    }

    @Override
    public Iterable<Path> getRootDirectories() {
        return List.of(rootPath);
    }

    @Override
    public Iterable<FileStore> getFileStores() {
        return List.of(store);
    }

    @Override
    public Set<String> supportedFileAttributeViews() {
        return ATTRIBUTE_VIEWS;
    }

    @Override
    public CopyOnWriteFSPath getPath(String first, String... more) {
        return CopyOnWriteFSPath.of(this, first, more);
    }

    @Override
    public PathMatcher getPathMatcher(String syntaxAndPattern) {
        return backingFileSystem().getPathMatcher(syntaxAndPattern);
    }

    @Override
    public UserPrincipalLookupService getUserPrincipalLookupService() {
        throw new UnsupportedOperationException("CowFS has no principals");
    }

    @Override
    public WatchService newWatchService() {
        throw new UnsupportedOperationException("CowFS cannot be watched");
    }

    public CopyOnWriteFileStore store() {
        return store;
    }

    public CopyOnWriteFSPath rootPath() {
        return rootPath;
    }

    DirectoryNode fileTree() {
        return fileTree;
    }

    public Path baseDirectory() {
        return baseDirectory;
    }

    public Path tmpDirectory() {
        return tmpDirectory;
    }

    Path createTemporaryFilePath() {
        return tmpDirectory.resolve("tmp_" + tmpFileIndex.incrementAndGet());
    }

    public FileSystem backingFileSystem() {
        return tmpDirectory.getFileSystem();
    }

    public Moves collectMoveOperations(Path outPath) {
        Moves moves = new Moves(new ArrayList<>(), new ArrayList<>(), new ArrayList<>());
        collectMoveOperations(outPath, fileTree, moves);
        return moves;
    }

    private void collectMoveOperations(Path outPath, DirectoryNode folder, Moves moves) {
        for (Node child : folder.children()) {
            Path target = outPath.resolve(Objects.requireNonNull(child.name()));
            if (child instanceof FileNode file) {
                (file.isCopy ? moves.copiedFiles : moves.preexistingFiles).add(new FileMove(file.storagePath(), target));
            } else if (child instanceof DirectoryNode directory) {
                moves.directories.add(target);
                collectMoveOperations(target, directory, moves);
            }
        }
    }

    public static void createDirectories(List<Path> directories) throws IOException {
        for (Path directory : directories) {
            if (!FileNatives.makeDirectories(directory.toString())) {
                throw new IOException("Cannot create directory " + directory);
            }
        }
    }

    public static void hardLinkFiles(List<FileMove> moves) throws IOException {
        for (FileMove move : moves) {
            if (FileNatives.stat(move.to().toString()) != null) {
                continue;
            }
            long[] source = FileNatives.stat(move.from().toString());
            if (source == null || source[0] != 0) {
                throw new IllegalStateException("Not a regular file: " + move.from());
            }
            if (!FileNatives.link(move.from().toString(), move.to().toString())) {
                throw new IOException("Cannot link " + move.from() + " to " + move.to());
            }
        }
    }

    public static void moveFiles(List<FileMove> moves) throws IOException {
        for (FileMove move : moves) {
            if (!FileNatives.move(move.from().toString(), move.to().toString(), false)) {
                throw new IOException("Cannot move " + move.from() + " to " + move.to());
            }
        }
    }

    public static void moveFilesWithRetry(List<FileMove> moves, CopyOption... options) throws IOException {
        boolean atomic = isAtomic(options);
        for (FileMove move : moves) {
            boolean fromExists = FileNatives.stat(move.from().toString()) != null;
            if (!fromExists && FileNatives.stat(move.to().toString()) != null) {
                continue;
            }
            long[] source = FileNatives.stat(move.from().toString());
            if (source == null || source[0] != 0) {
                throw new IOException("Not a regular file: " + move.from());
            }
            if (!FileNatives.move(move.from().toString(), move.to().toString(), atomic)) {
                throw new IOException("Cannot move " + move.from() + " to " + move.to());
            }
        }
    }

    public static List<FileMove> tryRevertMoves(List<FileMove> moves, CopyOption... options) {
        List<FileMove> failed = new ArrayList<>();
        for (FileMove move : moves) {
            boolean toExists = FileNatives.stat(move.to().toString()) != null;
            boolean fromExists = FileNatives.stat(move.from().toString()) != null;
            if (!toExists && fromExists) {
                continue;
            }
            long[] moved = FileNatives.stat(move.to().toString());
            if (moved != null && moved[0] == 0) {
                if (Util.safeMoveFile(move.to(), move.from(), options)) {
                    LOGGER.info("Reverted move from {} to {}", move.from(), move.to());
                } else {
                    LOGGER.error("Failed to revert move from {} to {}", move.from(), move.to());
                    failed.add(move);
                }
            } else {
                LOGGER.error("Skipping reverting move from {} to {} as it's not a file", move.from(), move.to());
                failed.add(move);
            }
        }
        if (failed.isEmpty()) {
            LOGGER.info("All moves were reverted, the world is back in its previous state");
        } else {
            LOGGER.error("Some moves could not be reverted");
        }
        return failed;
    }

    private static boolean isAtomic(CopyOption[] options) {
        for (CopyOption option : options) {
            if (option == java.nio.file.StandardCopyOption.ATOMIC_MOVE) {
                return true;
            }
        }
        return false;
    }

    public record Moves(List<Path> directories, List<FileMove> copiedFiles, List<FileMove> preexistingFiles) {}
}
