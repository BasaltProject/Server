// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.resources;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParseException;
import com.mojang.serialization.JsonOps;
import net.minecraft.util.GsonHelper;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import net.minecraft.server.packs.metadata.MetadataSectionType;
public interface ResourceMetadata {
    ResourceMetadata EMPTY = new ResourceMetadata() {
        @Override
        public <T> Optional<T> getSection(MetadataSectionType<T> serializer) {
            return Optional.empty();
        }
    };
    IoSupplier<ResourceMetadata> EMPTY_SUPPLIER = () -> EMPTY;
    static ResourceMetadata fromJsonStream(InputStream inputStream) throws IOException {
        try (BufferedReader reader = new BufferedReader(new java.io.InputStreamReader(inputStream, java.nio.charset.StandardCharsets.UTF_8))) {
            JsonObject root = GsonHelper.parse(reader);
            return new ResourceMetadata() {
                @Override
                public <T> Optional<T> getSection(MetadataSectionType<T> serializer) {
                    JsonElement raw = root.get(serializer.name());
                    return raw == null ? Optional.empty() : Optional.of(serializer.codec().parse(JsonOps.INSTANCE, raw).getOrThrow(JsonParseException::new));
                }
            };
        }
    }
    <T> Optional<T> getSection(MetadataSectionType<T> serializer);
    default <T> Optional<MetadataSectionType.WithValue<T>> getTypedSection(MetadataSectionType<T> type) {
        return getSection(type).map(type::withValue);
    }
    static <T> ResourceMetadata of(MetadataSectionType<T> k, T v) {
        return new MapBased(Map.of(k, v));
    }
    static <T1, T2> ResourceMetadata of(MetadataSectionType<T1> k1, T1 v1, MetadataSectionType<T2> k2, T2 v2) {
        return new MapBased(Map.of(k1, v1, k2, v2));
    }
    default List<MetadataSectionType.WithValue<?>> getTypedSections(Collection<MetadataSectionType<?>> types) {
        return types.stream().map(this::getTypedSection).flatMap(Optional::stream).collect(java.util.stream.Collectors.toUnmodifiableList());
    }
    public static class MapBased implements ResourceMetadata {
        private final Map<MetadataSectionType<?>, ?> values;

        private MapBased(Map<MetadataSectionType<?>, ?> values) {
            this.values = values;
            
        }
        public <T> Optional<T> getSection(MetadataSectionType<T> serializer) {
            return Optional.ofNullable((T) values.get(serializer));
        }
    }
}
