// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs;
import java.io.IOException;
import java.util.Enumeration;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.io.File;
import java.io.InputStream;
import java.nio.file.Path;
import java.util.Set;
import java.util.stream.Stream;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.repository.Pack;
import net.minecraft.server.packs.resources.IoSupplier;
public class FilePackResources extends AbstractPackMetadataResources implements PackResources {
    private final SharedZipFileAccess access;
    private final String prefix;

    private String prefixed(String path) {
        return prefix.isEmpty() ? path : prefix + "/" + path;
    }

    private IoSupplier<InputStream> entry(String path) {
        ZipFile zip = access.open();
        ZipEntry found = zip == null ? null : zip.getEntry(prefixed(path));
        return found == null ? null : IoSupplier.create(zip, found);
    }

    private FilePackResources(PackLocationInfo location, SharedZipFileAccess zipFileAccess, String prefix) {
        super(location);
        access = zipFileAccess;
        this.prefix = prefix;
    }
    public IoSupplier<InputStream> getRootResource(String... path) {
        return entry(String.join("/", path));
    }
    public IoSupplier<InputStream> getResource(PackType type, Identifier location) {
        return entry(type.getDirectory() + "/" + location.getNamespace() + "/" + location.getPath());
    }
    public Set<String> getNamespaces(PackType type) {
        ZipFile zip = access.open();
        if (zip == null) {
            return Set.of();
        }
        Set<String> found = new java.util.HashSet<>();
        String base = prefixed(type.getDirectory() + "/");
        for (Enumeration<? extends ZipEntry> entries = zip.entries(); entries.hasMoreElements(); ) {
            String namespace = extractNamespace(base, entries.nextElement().getName());
            if (!namespace.isEmpty() && Identifier.isValidNamespace(namespace)) {
                found.add(namespace);
            }
        }
        return found;
    }
    public static String extractNamespace(String prefix, String name) {
        if (!name.startsWith(prefix)) {
            return "";
        }
        int slash = name.indexOf('/', prefix.length());
        return slash < 0 ? name.substring(prefix.length()) : name.substring(prefix.length(), slash);
    }
    public void close() {
        access.shut();
    }
    public void listResources(PackType type, String namespace, String directory, PackResources.ResourceOutput output) {
        ZipFile zip = access.open();
        if (zip == null) {
            return;
        }
        String root = prefixed(type.getDirectory() + "/" + namespace + "/");
        String wanted = root + directory + "/";
        for (Enumeration<? extends ZipEntry> entries = zip.entries(); entries.hasMoreElements(); ) {
            ZipEntry zipEntry = entries.nextElement();
            if (zipEntry.isDirectory() || !zipEntry.getName().startsWith(wanted)) {
                continue;
            }
            Identifier id = Identifier.tryBuild(namespace, zipEntry.getName().substring(root.length()));
            if (id != null) {
                output.accept(id, IoSupplier.create(zip, zipEntry));
            }
        }
    }
    static class SharedZipFileAccess implements AutoCloseable {
        private final File file;
        private ZipFile zip;
        private boolean broken;

        ZipFile open() {
            if (broken) {
                return null;
            }
            if (zip == null) {
                try {
                    zip = new ZipFile(file);
                } catch (IOException failure) {
                    broken = true;
                }
            }
            return zip;
        }

        void shut() {
            if (zip != null) {
                try {
                    zip.close();
                } catch (IOException ignored) {
                    zip = null;
                }
                zip = null;
            }
        }

        private SharedZipFileAccess(File file) {
            this.file = file;
            
        }
        public void close() {
            shut();
        }
        protected void finalize() throws Throwable {
            shut();
            super.finalize();
        }
    }
    public static class FileResourcesSupplier implements Pack.ResourcesSupplier {
        private final File content;

        public FileResourcesSupplier(Path content) {
            this(content.toFile());
            
        }
        public FileResourcesSupplier(File content) {
            this.content = content;
            
        }
        public PackMetadataResources openMetadata(PackLocationInfo location) {
            return new FilePackResources(location, new SharedZipFileAccess(content), "");
        }
        public Stream<PackResources> openResources(PackLocationInfo location, Pack.Metadata metadata) {
            SharedZipFileAccess shared = new SharedZipFileAccess(content);
            PackResources primary = new FilePackResources(location, shared, "");
            if (metadata.overlays().isEmpty()) {
                return Stream.of(primary);
            }
            return Stream.of(new OverlayedPackResources(primary, metadata.overlays().stream().<PackResources>map(name -> new FilePackResources(location, shared, name)).toList()));
        }
    }
}
