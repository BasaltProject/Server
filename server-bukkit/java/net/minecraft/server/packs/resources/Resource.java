// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.resources;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.util.Optional;
import net.minecraft.server.packs.PackResources;
import net.minecraft.server.packs.repository.KnownPack;
public class Resource {
    private final PackResources source;
    private final IoSupplier<InputStream> streamSupplier;
    private final IoSupplier<ResourceMetadata> metadataSupplier;
    private ResourceMetadata cachedMetadata;

    public Resource(PackResources source, IoSupplier<InputStream> streamSupplier, IoSupplier<ResourceMetadata> metadataSupplier) {
        this.source = source;
        this.streamSupplier = streamSupplier;
        this.metadataSupplier = metadataSupplier;
        
    }
    public Resource(PackResources source, IoSupplier<InputStream> streamSupplier) {
        this.source = source;
        this.streamSupplier = streamSupplier;
        metadataSupplier = ResourceMetadata.EMPTY_SUPPLIER;
        cachedMetadata = ResourceMetadata.EMPTY;
        
    }
    public PackResources source() {
        return source;
    }
    public String sourcePackId() {
        return source.packId();
    }
    public Optional<KnownPack> knownPackInfo() {
        return source.knownPackInfo();
    }
    public InputStream open() throws IOException {
        return streamSupplier.get();
    }
    public BufferedReader openAsReader() throws IOException {
        return new BufferedReader(new java.io.InputStreamReader(open(), java.nio.charset.StandardCharsets.UTF_8));
    }
    public String readAllAsString() throws IOException {
        try (BufferedReader reader = openAsReader()) {
            return reader.lines().collect(java.util.stream.Collectors.joining("\n"));
        }
    }
    public ResourceMetadata metadata() throws IOException {
        if (cachedMetadata == null) {
            cachedMetadata = metadataSupplier.get();
        }
        return cachedMetadata;
    }
}
