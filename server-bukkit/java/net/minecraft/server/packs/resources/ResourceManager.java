// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.resources;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.stream.Stream;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.PackResources;
public interface ResourceManager extends ResourceProvider {
    Set<String> getNamespaces();
    List<Resource> getResourceStack(Identifier location);
    Map<Identifier, Resource> listResources(String directory, Selector selector);
    Map<Identifier, List<Resource>> listResourceStacks(String directory, Selector selector);
    Stream<PackResources> listPacks();
    public static enum Empty implements ResourceManager {
        INSTANCE;
        public Set<String> getNamespaces() {
            return Set.of();
        }
        public Optional<Resource> getResource(Identifier location) {
            return Optional.empty();
        }
        public List<Resource> getResourceStack(Identifier location) {
            return List.of();
        }
        public Map<Identifier, Resource> listResources(String directory, Selector selector) {
            return Map.of();
        }
        public Map<Identifier, List<Resource>> listResourceStacks(String directory, Selector selector) {
            return Map.of();
        }
        public Stream<PackResources> listPacks() {
            return Stream.empty();
        }
    }
    public interface Selector {
        boolean isIncluded(Identifier resourceId);
    }
}
