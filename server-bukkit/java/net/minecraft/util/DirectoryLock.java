// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.channels.FileLock;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;

public class DirectoryLock implements AutoCloseable {
    public static final String LOCK_FILE = "session.lock";
    private static final ByteBuffer kMarker = ByteBuffer.wrap("☃".getBytes(StandardCharsets.UTF_8));
    private final FileChannel lockFile;
    private final FileLock lock;

    public static DirectoryLock create(Path dir) throws IOException {
        Path lockPath = dir.resolve(LOCK_FILE);
        FileUtil.createDirectoriesSafe(dir);
        FileChannel channel = FileChannel.open(lockPath, StandardOpenOption.CREATE, StandardOpenOption.WRITE);
        try {
            channel.write(kMarker.duplicate());
            channel.force(true);
            FileLock held = channel.tryLock();
            if (held == null) {
                throw LockException.alreadyLocked(lockPath);
            }
            return new DirectoryLock(channel, held);
        } catch (IOException e) {
            try {
                channel.close();
            } catch (IOException nested) {
                e.addSuppressed(nested);
            }
            throw e;
        }
    }

    private DirectoryLock(FileChannel lockFile, FileLock lock) {
        this.lockFile = lockFile;
        this.lock = lock;
    }

    @Override
    public void close() throws IOException {
        try {
            if (lock.isValid()) {
                lock.release();
            }
        } finally {
            if (lockFile.isOpen()) {
                lockFile.close();
            }
        }
    }

    public boolean isValid() {
        return lock.isValid();
    }

    public static boolean isLocked(Path dir) throws IOException {
        Path lockPath = dir.resolve(LOCK_FILE);
        try (FileChannel channel = FileChannel.open(lockPath, StandardOpenOption.WRITE)) {
            FileLock probe = channel.tryLock();
            if (probe == null) {
                return true;
            }
            probe.close();
            return false;
        } catch (java.nio.channels.OverlappingFileLockException e) {
            return true;
        } catch (java.nio.file.NoSuchFileException e) {
            return false;
        }
    }

    public static class LockException extends IOException {
        private LockException(Path path, String message) {
            super(path.toAbsolutePath() + ": " + message);
        }

        public static LockException alreadyLocked(Path path) {
            return new LockException(path, "already locked (possibly by other Minecraft instance?)");
        }
    }
}
