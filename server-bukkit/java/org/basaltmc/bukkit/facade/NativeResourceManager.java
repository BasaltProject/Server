package org.basaltmc.bukkit.facade;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.stream.Stream;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.PackResources;
import net.minecraft.server.packs.resources.IoSupplier;
import net.minecraft.server.packs.resources.Resource;
import net.minecraft.server.packs.resources.ResourceManager;
import org.basaltmc.bukkit.LeftoverNatives;

/** The files of the enabled data packs, served by the native resource service. */
public final class NativeResourceManager implements ResourceManager {
    private static final String kRoot = "data/";

    private static String nameOf(Identifier id) {
        return kRoot + id.getNamespace() + "/" + id.getPath();
    }

    private static Identifier idOf(String name) {
        String rest = name.substring(kRoot.length());
        int slash = rest.indexOf('/');
        return slash < 0 ? null : Identifier.fromNamespaceAndPath(rest.substring(0, slash), rest.substring(slash + 1));
    }

    private static Resource open(String name) {
        return new NativeResource(name);
    }

    @Override
    public Set<String> getNamespaces() {
        Set<String> out = new LinkedHashSet<>();
        for (String name : LeftoverNatives.resourcesUnder(kRoot)) {
            Identifier id = idOf(name);
            if (id != null) {
                out.add(id.getNamespace());
            }
        }
        return out;
    }

    @Override
    public Optional<Resource> getResource(Identifier location) {
        String name = nameOf(location);
        return LeftoverNatives.resourcePack(name) == null ? Optional.empty() : Optional.of(open(name));
    }

    @Override
    public List<Resource> getResourceStack(Identifier location) {
        return getResource(location).map(List::of).orElse(List.of());
    }

    @Override
    public Map<Identifier, Resource> listResources(String directory, Selector selector) {
        Map<Identifier, Resource> out = new LinkedHashMap<>();
        for (String name : LeftoverNatives.resourcesUnder(kRoot)) {
            Identifier id = idOf(name);
            if (id != null && id.getPath().startsWith(directory + "/") && selector.isIncluded(id)) {
                out.put(id, open(name));
            }
        }
        return out;
    }

    @Override
    public Map<Identifier, List<Resource>> listResourceStacks(String directory, Selector selector) {
        Map<Identifier, List<Resource>> out = new LinkedHashMap<>();
        listResources(directory, selector).forEach((id, resource) -> out.put(id, new ArrayList<>(List.of(resource))));
        return out;
    }

    @Override
    public Stream<PackResources> listPacks() {
        return Stream.empty();
    }

    private static final class NativeResource extends Resource {
        private final String name;

        NativeResource(String name) {
            super(null, (IoSupplier<InputStream>) null);
            this.name = name;
        }

        @Override
        public String sourcePackId() {
            String pack = LeftoverNatives.resourcePack(name);
            return pack == null ? "unknown" : pack;
        }

        @Override
        public InputStream open() throws IOException {
            byte[] bytes = LeftoverNatives.resourceRead(name);
            if (bytes == null) {
                throw new java.io.FileNotFoundException(name);
            }
            return new ByteArrayInputStream(bytes);
        }

        @Override
        public java.io.BufferedReader openAsReader() throws IOException {
            return new java.io.BufferedReader(new java.io.InputStreamReader(open(), java.nio.charset.StandardCharsets.UTF_8));
        }
    }
}
