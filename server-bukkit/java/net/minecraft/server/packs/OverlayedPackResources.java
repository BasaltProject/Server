// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.io.IOException;
import java.io.InputStream;
import java.util.List;
import java.util.Set;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.metadata.MetadataSectionType;
import net.minecraft.server.packs.resources.IoSupplier;
public class OverlayedPackResources implements PackResources {
    private final PackMetadataResources primary;
    private final List<PackResources> stack;

    public OverlayedPackResources(PackResources primaryPackResources, List<PackResources> overlayPackResources) {
        primary = primaryPackResources;
        List<PackResources> layers = new java.util.ArrayList<>(overlayPackResources);
        java.util.Collections.reverse(layers);
        layers.add(primaryPackResources);
        stack = List.copyOf(layers);
        
    }
    public IoSupplier<InputStream> getRootResource(String... path) {
        return primary.getRootResource(path);
    }
    public IoSupplier<InputStream> getResource(PackType type, Identifier location) {
        for (PackResources layer : stack) {
            IoSupplier<InputStream> found = layer.getResource(type, location);
            if (found != null) {
                return found;
            }
        }
        return null;
    }
    public void listResources(PackType type, String namespace, String directory, PackResources.ResourceOutput output) {
        Map<Identifier, IoSupplier<InputStream>> merged = new HashMap<>();
        for (PackResources layer : stack) {
            layer.listResources(type, namespace, directory, merged::putIfAbsent);
        }
        merged.forEach(output);
    }
    public Set<String> getNamespaces(PackType type) {
        Set<String> all = new HashSet<>();
        for (PackResources layer : stack) {
            all.addAll(layer.getNamespaces(type));
        }
        return all;
    }
    public <T> T getMetadataSection(MetadataSectionType<T> metadataSerializer) throws IOException {
        return primary.getMetadataSection(metadataSerializer);
    }
    public PackLocationInfo location() {
        return primary.location();
    }
    public void close() {
        stack.forEach(PackMetadataResources::close);
    }
}
