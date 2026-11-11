// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;
import com.mojang.serialization.Codec;
import com.mojang.serialization.JavaOps;
import java.util.HashMap;
import java.util.Map;
import net.minecraft.resources.ResourceKey;
public class Cloner<T> {
    private final Codec<T> codec;
    private Cloner(Codec<T> directCodec) {
        this.codec = directCodec;
    }
    public T clone(T value, HolderLookup.Provider from, HolderLookup.Provider to) {
        Object tree = codec.encodeStart(from.createSerializationContext(JavaOps.INSTANCE), value)
                .getOrThrow(e -> new IllegalStateException("Failed to encode: " + e));
        return codec.parse(to.createSerializationContext(JavaOps.INSTANCE), tree)
                .getOrThrow(e -> new IllegalStateException("Failed to decode: " + e));
    }
    public static class Factory {
        private final Map<ResourceKey<? extends Registry<?>>, Cloner<?>> byRegistry = new HashMap<>();
        public <T> Factory addCodec(ResourceKey<? extends Registry<? extends T>> key, Codec<T> codec) {
            byRegistry.put(key, new Cloner<>(codec));
            return this;
        }
        @SuppressWarnings("unchecked")
        public <T> Cloner<T> cloner(ResourceKey<? extends Registry<? extends T>> key) {
            return (Cloner<T>) byRegistry.get(key);
        }
    }
}
