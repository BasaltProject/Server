// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data;

import com.google.common.hash.HashCode;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

public interface CachedOutput {
    CachedOutput NO_CACHE = (path, input, hash) -> {
        Path parent = path.getParent();
        if (parent != null) {
            Files.createDirectories(parent);
        }
        Files.write(path, input);
    };

    void writeIfNeeded(Path path, byte[] input, HashCode hash) throws IOException;
}
