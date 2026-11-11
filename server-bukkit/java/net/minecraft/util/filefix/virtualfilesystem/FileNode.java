// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.virtualfilesystem;

import java.io.IOException;
import java.nio.file.Path;
import org.basaltmc.bukkit.FileNatives;

final class FileNode extends Node {
    private Path storagePath;
    boolean isCopy;

    FileNode(CopyOnWriteFSPath path, Path storagePath, boolean isCopy) {
        super(path);
        this.storagePath = storagePath;
        this.isCopy = isCopy;
    }

    public void ensureCopy() throws IOException {
        if (isCopy) {
            return;
        }
        Path copy = path.getFileSystem().createTemporaryFilePath();
        if (!FileNatives.copy(storagePath.toString(), copy.toString())) {
            throw new IOException("Failed to copy " + storagePath + " to " + copy);
        }
        storagePath = copy;
        isCopy = true;
    }

    public void deleteCopy() {
        if (isCopy) {
            FileNatives.remove(storagePath.toString());
        }
    }

    public Path storagePath() {
        return storagePath;
    }
}
