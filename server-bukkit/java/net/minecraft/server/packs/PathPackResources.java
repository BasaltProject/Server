// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs;
import java.io.IOException;
import java.nio.file.*;
import java.util.HashSet;
import java.util.stream.StreamSupport;
import net.minecraft.util.FileUtil;
import java.io.InputStream;
import java.nio.file.Path;
import java.util.List;
import java.util.Set;
import java.util.stream.Stream;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.repository.Pack;
import net.minecraft.server.packs.resources.IoSupplier;
public class PathPackResources extends AbstractPackMetadataResources implements PackResources {
    private final Path root;

    public PathPackResources(PackLocationInfo location, Path root) {
        super(location);
        this.root = root;
    }
    public IoSupplier<InputStream> getRootResource(String... path) {
        FileUtil.validatePath(path);
        Path target = FileUtil.resolvePath(root, List.of(path));
        return Files.exists(target) ? IoSupplier.create(target) : null;
    }
    public static boolean validatePath(Path path) {
        return true;
    }
    public IoSupplier<InputStream> getResource(PackType type, Identifier location) {
        return getResource(root.resolve(type.getDirectory()), location);
    }
    public static IoSupplier<InputStream> getResource(Path topDir, Identifier location) {
        List<String> segments = FileUtil.decomposePath(location.getPath()).result().orElse(null);
        if (segments == null) {
            return null;
        }
        Path file = FileUtil.resolvePath(topDir.resolve(location.getNamespace()), segments);
        return Files.exists(file) && validatePath(file) ? IoSupplier.create(file) : null;
    }
    public void listResources(PackType type, String namespace, String directory, PackResources.ResourceOutput output) {
        listResources(root.resolve(type.getDirectory()), namespace, directory, output);
    }
    public static void listResources(Path topPath, String namespace, String directory, PackResources.ResourceOutput output) {
        FileUtil.decomposePath(directory).result().ifPresent(segments -> listPath(namespace, topPath.resolve(namespace), segments, output));
    }
    public static void listPath(String namespace, Path topDir, List<String> decomposedPrefixPath, PackResources.ResourceOutput output) {
        Path start = FileUtil.resolvePath(topDir, decomposedPrefixPath);
        try (Stream<Path> found = Files.find(start, Integer.MAX_VALUE, (file, attributes) -> attributes.isRegularFile())) {
            found.forEach(file -> {
                Identifier id = Identifier.tryBuild(namespace, String.join("/", StreamSupport.stream(topDir.relativize(file).spliterator(), false).map(Path::toString).toList()));
                if (id != null) {
                    output.accept(id, IoSupplier.create(file));
                }
            });
        } catch (IOException ignored) {
            return;
        }
    }
    public Set<String> getNamespaces(PackType type) {
        return getNamespaces(root.resolve(type.getDirectory()));
    }
    public static Set<String> getNamespaces(Path rootDir) {
        Set<String> found = new HashSet<>();
        try (DirectoryStream<Path> children = Files.newDirectoryStream(rootDir)) {
            for (Path child : children) {
                String name = child.getFileName().toString();
                if (Files.isDirectory(child) && Identifier.isValidNamespace(name)) {
                    found.add(name);
                }
            }
        } catch (IOException ignored) {
            return found;
        }
        return found;
    }
    public void close() {
        return;
    }
    public static class PathResourcesSupplier implements Pack.ResourcesSupplier {
        private final Path content;

        public PathResourcesSupplier(Path content) {
            this.content = content;
            
        }
        public PackMetadataResources openMetadata(PackLocationInfo location) {
            return new PathPackResources(location, content);
        }
        public Stream<PackResources> openResources(PackLocationInfo location, Pack.Metadata metadata) {
            PackResources primary = new PathPackResources(location, content);
            if (metadata.overlays().isEmpty()) {
                return Stream.of(primary);
            }
            List<PackResources> overlays = metadata.overlays().stream().<PackResources>map(name -> new PathPackResources(location, content.resolve(name))).toList();
            return Stream.of(new OverlayedPackResources(primary, overlays));
        }
    }
}
