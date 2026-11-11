// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.JavaOps;
import java.util.HashMap;
import java.util.Map;
import java.util.Optional;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.HolderSet;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.resources.RegistryOps;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;

public class PlaceholderLookupProvider implements HolderGetter.Provider {
    private final HolderLookup.Provider context;
    private final UniversalLookup lookup = new UniversalLookup();
    private final Map<ResourceKey<Object>, Holder.Reference<Object>> holders = new HashMap<>();
    private final Map<TagKey<Object>, HolderSet.Named<Object>> holderSets = new HashMap<>();

    public PlaceholderLookupProvider(HolderLookup.Provider context) {
        this.context = context;
    }

    @Override
    public <T> Optional<? extends HolderGetter<T>> lookup(ResourceKey<? extends Registry<? extends T>> key) {
        return Optional.of(lookup.castAsLookup());
    }

    public <V> RegistryOps<V> createSerializationContext(DynamicOps<V> parent) {
        return RegistryOps.create(parent, new RegistryOps.RegistryInfoLookup() {
            @Override
            public <T> Optional<HolderGetter<T>> lookup(ResourceKey<? extends Registry<? extends T>> registryKey) {
                Optional<HolderGetter<T>> found = context.lookup(registryKey).map(getter -> getter);
                return found.or(() -> Optional.of(lookup.castAsLookup()));
            }
        });
    }

    public RegistryContextSwapper createSwapper() {
        return new RegistryContextSwapper() {
            @Override
            public <T> DataResult<T> swapTo(Codec<T> codec, T value, HolderLookup.Provider newContext) {
                return codec.encodeStart(createSerializationContext(JavaOps.INSTANCE), value)
                    .flatMap(encoded -> codec.parse(newContext.createSerializationContext(JavaOps.INSTANCE), encoded));
            }
        };
    }

    public boolean hasRegisteredPlaceholders() {
        return !holders.isEmpty() || !holderSets.isEmpty();
    }

    private class UniversalLookup implements HolderGetter<Object> {
        @Override
        public Optional<Holder.Reference<Object>> get(ResourceKey<Object> id) {
            return Optional.of(getOrThrow(id));
        }

        @Override
        public Holder.Reference<Object> getOrThrow(ResourceKey<Object> id) {
            return holders.computeIfAbsent(id, key -> Holder.Reference.createStandAlone(this, key));
        }

        @Override
        public Optional<HolderSet.Named<Object>> get(TagKey<Object> id) {
            return Optional.of(getOrThrow(id));
        }

        @Override
        public HolderSet.Named<Object> getOrThrow(TagKey<Object> id) {
            return holderSets.computeIfAbsent(id, key -> HolderSet.emptyNamed(this, key));
        }

        @SuppressWarnings("unchecked")
        public <T> HolderGetter<T> castAsLookup() {
            return (HolderGetter<T>) this;
        }
    }
}
