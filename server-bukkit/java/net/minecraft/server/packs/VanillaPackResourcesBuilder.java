// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs;
import java.net.*;
import java.nio.file.Paths;
import java.util.*;
import java.util.stream.Collectors;
import java.nio.file.Path;
import java.util.function.Consumer;
import net.minecraft.server.packs.resources.ResourceMetadata;
public class VanillaPackResourcesBuilder {
    public static Consumer<VanillaPackResourcesBuilder> developmentConfig = builder -> {
    };
    private final FixedPathPackResources.Builder whole = new FixedPathPackResources.Builder();
    private final List<FixedPathPackResources.Builder> layers = new ArrayList<>();
    private final Set<String> exposed = new HashSet<>();
    private ResourceMetadata metadata;
    private static final Map<PackType, Path> ROOT_DIR_BY_TYPE = probeRoots();

    private static Map<PackType, Path> probeRoots() {
        Map<PackType, Path> roots = new EnumMap<>(PackType.class);
        for (PackType type : PackType.values()) {
            URL probe = VanillaPackResources.class.getResource("/" + type.getDirectory() + "/.mcassetsroot");
            if (probe == null) {
                continue;
            }
            try {
                roots.put(type, Paths.get(probe.toURI()).getParent());
            } catch (Exception ignored) {
                continue;
            }
        }
        return roots;
    }

    private void everyBuilder(Consumer<FixedPathPackResources.Builder> task) {
        task.accept(whole);
        layers.forEach(task);
    }

    private void pushRoot(Path path) {
        whole.pushRootPath(path);
        layers.getLast().pushRootPath(path);
    }

    private void pushType(PackType type, Path path) {
        whole.pushPathForType(type, path);
        layers.getLast().pushPathForType(type, path);
    }

    public VanillaPackResourcesBuilder() {
        layers.add(new FixedPathPackResources.Builder());
        
    }
    public VanillaPackResourcesBuilder pushJarResources() {
        ROOT_DIR_BY_TYPE.forEach((packType, path) -> {
            pushRoot(path.getParent());
            pushType(packType, path);
        });
        return this;
    }
    public VanillaPackResourcesBuilder pushClasspathResources(PackType packType, Class<?> source) {
        try {
            Enumeration<URL> found = source.getClassLoader().getResources(packType.getDirectory() + "/");
            while (found.hasMoreElements()) {
                URI uri = found.nextElement().toURI();
                if ("file".equals(uri.getScheme())) {
                    Path assets = Paths.get(uri);
                    pushRoot(assets.getParent());
                    pushType(packType, assets);
                }
            }
        } catch (Exception ignored) {
            return this;
        }
        return this;
    }
    public VanillaPackResourcesBuilder applyDevelopmentConfig() {
        developmentConfig.accept(this);
        return this;
    }
    public VanillaPackResourcesBuilder pushUniversalPath(Path path) {
        pushRoot(path);
        for (PackType type : PackType.values()) {
            pushType(type, path.resolve(type.getDirectory()));
        }
        return this;
    }
    public VanillaPackResourcesBuilder pushAssetPath(PackType packType, Path path) {
        pushRoot(path);
        pushType(packType, path);
        return this;
    }
    public VanillaPackResourcesBuilder setMetadata(ResourceMetadata metadata) {
        everyBuilder(builder -> builder.setMetadata(metadata));
        this.metadata = metadata;
        return this;
    }
    public VanillaPackResourcesBuilder exposeNamespace(String... namespaces) {
        List<String> names = List.of(namespaces);
        everyBuilder(builder -> builder.exposeNamespace(names));
        exposed.addAll(names);
        return this;
    }
    public VanillaPackResourcesBuilder pushLayer() {
        FixedPathPackResources.Builder next = new FixedPathPackResources.Builder();
        layers.add(next);
        next.exposeNamespace(exposed);
        if (metadata != null) {
            next.setMetadata(metadata);
        }
        return this;
    }
    public VanillaPackResources build(PackLocationInfo location) {
        return new VanillaPackResources(whole.build(location), layers.stream().map(builder -> builder.build(location)).collect(Collectors.toUnmodifiableList()));
    }
}
