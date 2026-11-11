// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs;
import java.io.InputStream;
import net.minecraft.server.packs.resources.IoSupplier;
import java.io.IOException;
import net.minecraft.server.packs.metadata.MetadataSectionType;
import net.minecraft.server.packs.resources.ResourceMetadata;
public abstract class AbstractPackMetadataResources implements PackMetadataResources {
    private final PackLocationInfo location;
    private ResourceMetadata metadata;

    protected AbstractPackMetadataResources(PackLocationInfo location) {
        this.location = location;
        
    }
    public <T> T getMetadataSection(MetadataSectionType<T> metadataSerializer) throws IOException {
        if (metadata == null) {
            metadata = loadMetadata(this);
        }
        return metadata.getSection(metadataSerializer).orElse(null);
    }
    public static ResourceMetadata loadMetadata(PackMetadataResources packResources) throws IOException {
        IoSupplier<InputStream> source = packResources.getRootResource("pack.mcmeta");
        if (source == null) {
            return ResourceMetadata.EMPTY;
        }
        try (InputStream stream = source.get()) {
            return ResourceMetadata.fromJsonStream(stream);
        }
    }
    public PackLocationInfo location() {
        return location;
    }
}
