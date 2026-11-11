// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.resources;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.stream.Stream;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.PackResources;
import net.minecraft.server.packs.PackType;
import net.minecraft.util.Unit;
public class ReloadableResourceManager implements AutoCloseable, ResourceManager {
    private CloseableResourceManager resources;
    private final List<PreparableReloadListener> listeners = new java.util.ArrayList<>();
    private final PackType type;

    public ReloadableResourceManager(PackType type) {
        this.type = type;
        resources = new MultiPackResourceManager(type, List.of());
        
    }
    public void close() {
        resources.close();
    }
    public void registerReloadListener(PreparableReloadListener listener) {
        listeners.add(listener);
    }
    public ReloadInstance createReload(Executor backgroundExecutor, Executor mainThreadExecutor, CompletableFuture<Unit> initialTask, List<PackResources> resourcePacks) {
        resources.close();
        resources = new MultiPackResourceManager(type, resourcePacks);
        return SimpleReloadInstance.create(resources, listeners, backgroundExecutor, mainThreadExecutor, initialTask, false);
    }
    public Optional<Resource> getResource(Identifier location) {
        return resources.getResource(location);
    }
    public Set<String> getNamespaces() {
        return resources.getNamespaces();
    }
    public List<Resource> getResourceStack(Identifier location) {
        return resources.getResourceStack(location);
    }
    public Map<Identifier, Resource> listResources(String directory, ResourceManager.Selector selector) {
        return resources.listResources(directory, selector);
    }
    public Map<Identifier, List<Resource>> listResourceStacks(String directory, ResourceManager.Selector selector) {
        return resources.listResourceStacks(directory, selector);
    }
    public Stream<PackResources> listPacks() {
        return resources.listPacks();
    }
}
