// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs;
import java.util.stream.Stream;
import net.minecraft.server.packs.resources.FallbackResourceManager;
import java.nio.file.Path;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.repository.Pack;
import net.minecraft.server.packs.resources.ResourceManager;
public class VanillaPackResources {
    private final FixedPathPackResources full;
    private final List<PackResources> layers;

    public VanillaPackResources(FixedPathPackResources fullResources, List<PackResources> resourceLayers) {
        full = fullResources;
        layers = resourceLayers;
        
    }
    public void listRawPaths(PackType type, Identifier resource, Consumer<Path> output) {
        full.listRawPaths(type, resource, output);
    }
    public PackResources fullResources() {
        return full;
    }
    public Pack.ResourcesSupplier asResourcesSupplier() {
        return new Pack.ResourcesSupplier() {
            @Override
            public PackMetadataResources openMetadata(PackLocationInfo location) {
                return full;
            }

            @Override
            public Stream<PackResources> openResources(PackLocationInfo location, Pack.Metadata metadata) {
                return layers.stream();
            }
        };
    }
    public ResourceManager asResourceManager() {
        FallbackResourceManager only = new FallbackResourceManager(PackType.CLIENT_RESOURCES, "minecraft");
        only.push(full);
        return only;
    }
}
