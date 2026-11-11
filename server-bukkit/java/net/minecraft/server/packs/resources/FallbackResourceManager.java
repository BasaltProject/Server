// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.resources;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.TreeMap;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.stream.Stream;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.PackResources;
import net.minecraft.server.packs.PackType;
public class FallbackResourceManager implements ResourceManager {
    private final List<PackEntry> layers = new ArrayList<>();
    private final PackType type;
    private final String namespace;

    private void add(String name, PackResources pack, PackResources.Filter filter) {
        layers.add(new PackEntry(name, pack, filter));
    }

    private static boolean isMeta(Identifier id) {
        return id.getPath().endsWith(".mcmeta");
    }

    private static Identifier stripMeta(Identifier id) {
        return id.withPath(id.getPath().substring(0, id.getPath().length() - ".mcmeta".length()));
    }

    private static Identifier metaLocation(Identifier id) {
        return id.withPath(id.getPath() + ".mcmeta");
    }

    private static ResourceMetadata readMetadata(IoSupplier<InputStream> source) throws IOException {
        try (InputStream stream = source.get()) {
            return ResourceMetadata.fromJsonStream(stream);
        }
    }

    private IoSupplier<ResourceMetadata> metadataFrom(Identifier location, int lowestLayer) {
        return () -> {
            Identifier metaId = metaLocation(location);
            for (int index = layers.size() - 1; index >= lowestLayer; index--) {
                PackEntry layer = layers.get(index);
                IoSupplier<InputStream> meta = layer.resources() == null ? null : layer.resources().getResource(type, metaId);
                if (meta != null) {
                    return readMetadata(meta);
                }
                if (layer.isFiltered(metaId)) {
                    break;
                }
            }
            return ResourceMetadata.EMPTY;
        };
    }

    public FallbackResourceManager(PackType type, String namespace) {
        this.type = type;
        this.namespace = namespace;
        
    }
    public void push(PackResources pack) {
        add(pack.packId(), pack, null);
    }
    public void push(PackResources pack, PackResources.Filter filter) {
        add(pack.packId(), pack, filter);
    }
    public void pushFilterOnly(String name, PackResources.Filter filter) {
        add(name, null, filter);
    }
    public Set<String> getNamespaces() {
        return Set.of(namespace);
    }
    public Optional<Resource> getResource(Identifier location) {
        for (int index = layers.size() - 1; index >= 0; index--) {
            PackEntry layer = layers.get(index);
            IoSupplier<InputStream> found = layer.resources() == null ? null : layer.resources().getResource(type, location);
            if (found != null) {
                return Optional.of(new Resource(layer.resources(), found, metadataFrom(location, index)));
            }
            if (layer.isFiltered(location)) {
                break;
            }
        }
        return Optional.empty();
    }
    public List<Resource> getResourceStack(Identifier location) {
        Identifier metaId = metaLocation(location);
        List<Resource> stack = new ArrayList<>();
        boolean metaHidden = false;
        for (int index = layers.size() - 1; index >= 0; index--) {
            PackEntry layer = layers.get(index);
            PackResources pack = layer.resources();
            IoSupplier<InputStream> found = pack == null ? null : pack.getResource(type, location);
            if (found != null) {
                boolean hidden = metaHidden;
                stack.add(0, new Resource(pack, found, hidden ? ResourceMetadata.EMPTY_SUPPLIER : () -> {
                    IoSupplier<InputStream> meta = pack.getResource(type, metaId);
                    return meta == null ? ResourceMetadata.EMPTY : readMetadata(meta);
                }));
            }
            if (layer.isFiltered(location)) {
                break;
            }
            metaHidden |= layer.isFiltered(metaId);
        }
        return stack;
    }
    public Map<Identifier, Resource> listResources(String directory, ResourceManager.Selector selector) {
        record Hit(PackResources pack, IoSupplier<InputStream> supplier, int layer) {
        }
        Map<Identifier, Hit> files = new HashMap<>();
        Map<Identifier, Hit> metas = new HashMap<>();
        for (int index = 0; index < layers.size(); index++) {
            PackEntry layer = layers.get(index);
            layer.filterAll(files.keySet());
            layer.filterAll(metas.keySet());
            PackResources pack = layer.resources();
            int layerIndex = index;
            if (pack != null) {
                pack.listResources(type, namespace, directory, (id, supplier) -> {
                    if (isMeta(id)) {
                        if (selector.isIncluded(stripMeta(id))) {
                            metas.put(id, new Hit(pack, supplier, layerIndex));
                        }
                    } else if (selector.isIncluded(id)) {
                        files.put(id, new Hit(pack, supplier, layerIndex));
                    }
                });
            }
        }
        Map<Identifier, Resource> merged = new TreeMap<>();
        files.forEach((id, hit) -> {
            Hit meta = metas.get(metaLocation(id));
            IoSupplier<ResourceMetadata> reader = meta != null && meta.layer() >= hit.layer() ? () -> readMetadata(meta.supplier()) : ResourceMetadata.EMPTY_SUPPLIER;
            merged.put(id, new Resource(hit.pack(), hit.supplier(), reader));
        });
        return merged;
    }
    public Map<Identifier, List<Resource>> listResourceStacks(String directory, ResourceManager.Selector selector) {
        Map<Identifier, EntryStack> found = new HashMap<>();
        for (PackEntry layer : layers) {
            for (EntryStack stack : found.values()) {
                if (layer.isFiltered(stack.fileLocation())) {
                    stack.fileSources().clear();
                } else if (layer.isFiltered(stack.metadataLocation())) {
                    stack.metaSources().clear();
                }
            }
            PackResources pack = layer.resources();
            if (pack == null) {
                continue;
            }
            pack.listResources(type, namespace, directory, (id, supplier) -> {
                if (isMeta(id)) {
                    Identifier base = stripMeta(id);
                    if (selector.isIncluded(base)) {
                        found.computeIfAbsent(base, EntryStack::new).metaSources().put(pack, supplier);
                    }
                } else if (selector.isIncluded(id)) {
                    found.computeIfAbsent(id, EntryStack::new).fileSources().add(new ResourceWithSource(pack, supplier));
                }
            });
        }
        Map<Identifier, List<Resource>> stacks = new TreeMap<>();
        for (EntryStack stack : found.values()) {
            if (stack.fileSources().isEmpty()) {
                continue;
            }
            List<Resource> resources = new ArrayList<>();
            for (ResourceWithSource source : stack.fileSources()) {
                IoSupplier<InputStream> meta = stack.metaSources().get(source.source());
                resources.add(new Resource(source.source(), source.resource(), meta != null ? () -> readMetadata(meta) : ResourceMetadata.EMPTY_SUPPLIER));
            }
            stacks.put(stack.fileLocation(), resources);
        }
        return stacks;
    }
    public Stream<PackResources> listPacks() {
        return layers.stream().map(PackEntry::resources).filter(java.util.Objects::nonNull);
    }
    static class LeakedResourceWarningInputStream extends FilterInputStream {
        private boolean closed;

        public LeakedResourceWarningInputStream(InputStream wrapped, Identifier location, String name) {
            super(wrapped);
        }
        public void close() throws IOException {
            super.close();
            closed = true;
        }
        protected void finalize() throws Throwable {
            super.finalize();
        }
    }
    static record EntryStack(Identifier fileLocation, Identifier metadataLocation, List<ResourceWithSource> fileSources, Map<PackResources, IoSupplier<InputStream>> metaSources) {
        public EntryStack(Identifier fileLocation) {
this(fileLocation, metaLocation(fileLocation), new ArrayList<>(), new HashMap<>());
        }
    }
    static record PackEntry(String name, PackResources resources, PackResources. Filter filter) {
        public void filterAll(Collection<Identifier> collection) {
            if (filter != null) {
                collection.removeIf(filter::isFiltered);
            }
        }
        public boolean isFiltered(Identifier location) {
            return filter != null && filter.isFiltered(location);
        }
    }
    static record ResourceWithSource(PackResources source, IoSupplier<InputStream> resource) {
    }
}
