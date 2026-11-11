// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.resources;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Path;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
public interface IoSupplier<T> {
    static IoSupplier<InputStream> create(Path path) {
        return () -> java.nio.file.Files.newInputStream(path);
    }
    static IoSupplier<InputStream> create(ZipFile zipFile, ZipEntry entry) {
        return () -> zipFile.getInputStream(entry);
    }
    T get() throws IOException;
}
