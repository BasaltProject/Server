// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.io.IOException;
import java.net.URI;
import java.nio.file.FileSystemAlreadyExistsException;
import java.nio.file.FileSystemNotFoundException;
import java.nio.file.FileSystems;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Collections;

public class FileSystemUtil {
    public static Path safeGetPath(URI probeUri) throws IOException {
        try {
            return Paths.get(probeUri);
        } catch (FileSystemNotFoundException | IllegalArgumentException ignored) {
            try {
                FileSystems.newFileSystem(probeUri, Collections.emptyMap());
            } catch (FileSystemAlreadyExistsException alreadyOpen) {
                return Paths.get(probeUri);
            }
            return Paths.get(probeUri);
        }
    }
}
