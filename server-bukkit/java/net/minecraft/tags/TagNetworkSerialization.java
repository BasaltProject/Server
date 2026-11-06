// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import io.netty.buffer.ByteBuf;
import it.unimi.dsi.fastutil.ints.IntArrayList;
import it.unimi.dsi.fastutil.ints.IntList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.LayeredRegistryAccess;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistrySynchronization;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.RegistryLayer;

public class TagNetworkSerialization {
    public static Map<ResourceKey<? extends Registry<?>>, NetworkPayload> serializeTagsToNetwork(LayeredRegistryAccess<RegistryLayer> registries) {
        Map<ResourceKey<? extends Registry<?>>, NetworkPayload> result = new HashMap<>();
        RegistrySynchronization.networkSafeRegistries(registries).forEach(entry -> {
            NetworkPayload payload = serializeToNetwork(entry.value());
            if (!payload.isEmpty()) {
                result.put(entry.key(), payload);
            }
        });
        return result;
    }

    private static <T> NetworkPayload serializeToNetwork(Registry<T> registry) {
        Map<Identifier, IntList> result = new HashMap<>();
        registry.getTags().forEach(tag -> {
            IntList ids = new IntArrayList();
            for (Holder<T> holder : tag) {
                ids.add(registry.getId(holder.value()));
            }
            result.put(tag.key().location(), ids);
        });
        return new NetworkPayload(result);
    }

    private static <T> TagLoader.LoadResult<T> deserializeTagsFromNetwork(Registry<T> registry, NetworkPayload payload) {
        ResourceKey<? extends Registry<T>> registryKey = registry.key();
        Map<TagKey<T>, List<Holder<T>>> tags = new HashMap<>();
        payload.tags.forEach((key, ids) -> {
            List<Holder<T>> values = new java.util.ArrayList<>();
            ids.forEach((int id) -> registry.get(id).ifPresent(values::add));
            tags.put(TagKey.create(registryKey, key), List.copyOf(values));
        });
        return new TagLoader.LoadResult<T>(registryKey, tags);
    }

    public static record NetworkPayload(Map<Identifier, IntList> tags) {
        public static final NetworkPayload EMPTY = new NetworkPayload(Map.of());
        private static final StreamCodec<ByteBuf, IntList> kIdList = ByteBufCodecs.VAR_INT.apply(ByteBufCodecs.collection(IntArrayList::new));
        public static final StreamCodec<ByteBuf, NetworkPayload> STREAM_CODEC = StreamCodec.composite(
            ByteBufCodecs.map(HashMap::new, Identifier.STREAM_CODEC, kIdList), NetworkPayload::tags, NetworkPayload::new);

        public boolean isEmpty() {
            return tags.isEmpty();
        }

        public int size() {
            return tags.size();
        }

        public <T> TagLoader.LoadResult<T> resolve(Registry<T> registry) {
            return deserializeTagsFromNetwork(registry, this);
        }
    }
}
