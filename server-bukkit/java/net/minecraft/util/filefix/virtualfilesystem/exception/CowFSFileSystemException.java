// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.virtualfilesystem.exception;
import java.nio.file.FileSystemException;
public class CowFSFileSystemException extends FileSystemException {
    public CowFSFileSystemException(String message) {
        super(message);
    }
}
