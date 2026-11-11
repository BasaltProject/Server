// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.resources;
import java.util.HashMap;
import java.util.TreeMap;
import net.minecraft.server.packs.PackMetadataResources;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.stream.Stream;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.PackResources;
import net.minecraft.server.packs.PackType;
public class MultiPackResourceManager implements CloseableResourceManager {
    private final Map<String, FallbackResourceManager> managers = new HashMap<>();
    private final List<PackResources> packs;

    private static void requireNoTrailingSlash(String directory) {
        if (directory.endsWith("/")) {
            throw new IllegalArgumentException("Trailing slash in path " + directory);
        }
    }

    private static ResourceFilterSection filterOf(PackResources pack) {
        try {
            return pack.getMetadataSection(ResourceFilterSection.TYPE);
        } catch (Exception failure) {
            return null;
        }
    }

    public MultiPackResourceManager(PackType type, List<PackResources> packs) {
        this.packs = List.copyOf(packs);
        List<String> known = packs.stream().flatMap(pack -> pack.getNamespaces(type).stream()).distinct().toList();
        for (PackResources pack : packs) {
            ResourceFilterSection filter = filterOf(pack);
            Set<String> provided = pack.getNamespaces(type);
            PackResources.Filter pathFilter = filter == null ? null : id -> filter.isPathFiltered(id.getPath());
            for (String namespace : known) {
                boolean has = provided.contains(namespace);
                boolean blocks = filter != null && filter.isNamespaceFiltered(namespace);
                if (!has && !blocks) {
                    continue;
                }
                FallbackResourceManager manager = managers.computeIfAbsent(namespace, name -> new FallbackResourceManager(type, name));
                if (has && blocks) {
                    manager.push(pack, pathFilter);
                } else if (has) {
                    manager.push(pack);
                } else {
                    manager.pushFilterOnly(pack.packId(), pathFilter);
                }
            }
        }
        
    }
    public Set<String> getNamespaces() {
        return managers.keySet();
    }
    public Optional<Resource> getResource(Identifier location) {
        FallbackResourceManager manager = managers.get(location.getNamespace());
        return manager == null ? Optional.empty() : manager.getResource(location);
    }
    public List<Resource> getResourceStack(Identifier location) {
        FallbackResourceManager manager = managers.get(location.getNamespace());
        return manager == null ? List.of() : manager.getResourceStack(location);
    }
    public Map<Identifier, Resource> listResources(String directory, ResourceManager.Selector selector) {
        requireNoTrailingSlash(directory);
        Map<Identifier, Resource> merged = new TreeMap<>();
        for (FallbackResourceManager manager : managers.values()) {
            merged.putAll(manager.listResources(directory, selector));
        }
        return merged;
    }
    public Map<Identifier, List<Resource>> listResourceStacks(String directory, ResourceManager.Selector selector) {
        requireNoTrailingSlash(directory);
        Map<Identifier, List<Resource>> merged = new TreeMap<>();
        for (FallbackResourceManager manager : managers.values()) {
            merged.putAll(manager.listResourceStacks(directory, selector));
        }
        return merged;
    }
    public Stream<PackResources> listPacks() {
        return packs.stream();
    }
    public void close() {
        packs.forEach(PackMetadataResources::close);
    }
}
