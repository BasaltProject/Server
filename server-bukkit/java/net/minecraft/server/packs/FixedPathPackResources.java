// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs;
import java.nio.file.Files;
import java.util.*;
import net.minecraft.util.FileUtil;
import java.io.InputStream;
import java.nio.file.Path;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.Consumer;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.metadata.MetadataSectionType;
import net.minecraft.server.packs.resources.IoSupplier;
import net.minecraft.server.packs.resources.ResourceMetadata;
public class FixedPathPackResources implements PackResources {
    private final PackLocationInfo location;
    private final ResourceMetadata builtIn;
    private ResourceMetadata diskMetadata;
    private final Set<String> namespaces;
    private final List<Path> rootPaths;
    private final Map<PackType, List<Path>> pathsForType;

    private FixedPathPackResources(PackLocationInfo location, ResourceMetadata metadata, Set<String> namespaces, List<Path> rootPaths, Map<PackType, List<Path>> pathsForType) {
        this.location = location;
        builtIn = metadata;
        this.namespaces = namespaces;
        this.rootPaths = rootPaths;
        this.pathsForType = pathsForType;
        
    }
    public IoSupplier<InputStream> getRootResource(String... path) {
        FileUtil.validatePath(path);
        List<String> segments = List.of(path);
        for (Path base : rootPaths) {
            Path candidate = FileUtil.resolvePath(base, segments);
            if (Files.exists(candidate) && PathPackResources.validatePath(candidate)) {
                return IoSupplier.create(candidate);
            }
        }
        return null;
    }
    public void listRawPaths(PackType type, Identifier resource, Consumer<Path> output) {
        FileUtil.decomposePath(resource.getPath()).result().ifPresent(segments -> {
            for (Path base : pathsForType.get(type)) {
                output.accept(FileUtil.resolvePath(base.resolve(resource.getNamespace()), segments));
            }
        });
    }
    public void listResources(PackType type, String namespace, String directory, PackResources.ResourceOutput output) {
        FileUtil.decomposePath(directory).result().ifPresent(segments -> {
            Map<Identifier, IoSupplier<InputStream>> seen = new LinkedHashMap<>();
            List<Path> bases = pathsForType.get(type);
            for (int index = 0; index < bases.size(); index++) {
                PathPackResources.listPath(namespace, bases.get(index).resolve(namespace), segments, seen::putIfAbsent);
            }
            seen.forEach(output);
        });
    }
    public IoSupplier<InputStream> getResource(PackType type, Identifier location) {
        List<String> segments = FileUtil.decomposePath(location.getPath()).result().orElse(null);
        if (segments == null) {
            return null;
        }
        for (Path base : pathsForType.get(type)) {
            Path candidate = FileUtil.resolvePath(base.resolve(location.getNamespace()), segments);
            if (Files.exists(candidate) && PathPackResources.validatePath(candidate)) {
                return IoSupplier.create(candidate);
            }
        }
        return null;
    }
    public Set<String> getNamespaces(PackType type) {
        return namespaces;
    }
    public <T> T getMetadataSection(MetadataSectionType<T> metadataSerializer) {
        try {
            if (diskMetadata == null) {
                diskMetadata = AbstractPackMetadataResources.loadMetadata(this);
            }
            Optional<T> section = diskMetadata.getSection(metadataSerializer);
            if (section.isPresent()) {
                return section.get();
            }
        } catch (Exception ignored) {
            return builtIn.getSection(metadataSerializer).orElse(null);
        }
        return builtIn.getSection(metadataSerializer).orElse(null);
    }
    public PackLocationInfo location() {
        return location;
    }
    public void close() {
        return;
    }
    public static class Builder {
        private final Set<Path> roots = new LinkedHashSet<>();
        private final Map<PackType, Set<Path>> byType = new EnumMap<>(PackType.class);
        private ResourceMetadata metadata = ResourceMetadata.EMPTY;
        private final Set<String> exposed = new HashSet<>();

        private static boolean isDirectory(Path path) {
            if (!Files.exists(path)) {
                return false;
            }
            if (!Files.isDirectory(path)) {
                throw new IllegalArgumentException("Path " + path.toAbsolutePath() + " is not directory");
            }
            return true;
        }

        private static List<Path> newestFirst(Collection<Path> paths) {
            List<Path> copy = new ArrayList<>(paths);
            Collections.reverse(copy);
            return List.copyOf(copy);
        }

        public Builder() {
            
        }
        public void pushRootPath(Path path) {
            if (isDirectory(path)) {
                roots.add(path);
            }
        }
        public void pushPathForType(PackType packType, Path path) {
            if (isDirectory(path)) {
                byType.computeIfAbsent(packType, key -> new LinkedHashSet<>()).add(path);
            }
        }
        public void setMetadata(ResourceMetadata metadata) {
            this.metadata = metadata;
        }
        public void exposeNamespace(Collection<String> namespaces) {
            exposed.addAll(namespaces);
        }
        public FixedPathPackResources build(PackLocationInfo location) {
            Map<PackType, List<Path>> ordered = new EnumMap<>(PackType.class);
            for (PackType type : PackType.values()) {
                ordered.put(type, newestFirst(byType.getOrDefault(type, Set.of())));
            }
            return new FixedPathPackResources(location, metadata, Set.copyOf(exposed), newestFirst(roots), ordered);
        }
    }
}
