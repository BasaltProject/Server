// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.Registry;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.Optional;
import net.minecraft.network.codec.StreamCodec;

public record TagKey<T>(ResourceKey<? extends Registry<T>> registry, Identifier location) {
    public static <T> TagKey<T> create(ResourceKey<? extends Registry<T>> registry, Identifier name) {
        return new TagKey<>(registry, name);
    }

    public boolean isFor(ResourceKey<? extends Registry<?>> registryKey) {
        return registry.identifier().equals(registryKey.identifier());
    }

    @Override
    public String toString() {
        return "TagKey[" + registry.identifier() + " / " + location + "]";
    }
    public static <T> Codec<TagKey<T>> codec(ResourceKey<? extends Registry<T>> registryName) {
        return Identifier.CODEC.xmap(name -> create(registryName, name), TagKey::location);
    }
    public static <T> Codec<TagKey<T>> hashedCodec(ResourceKey<? extends Registry<T>> registryName) {
        return com.mojang.serialization.Codec.STRING.comapFlatMap(text -> text.startsWith("#")
            ? Identifier.read(text.substring(1)).map(name -> create(registryName, name))
            : com.mojang.serialization.DataResult.error(() -> "Not a tag id"), key -> "#" + key.location());
    }
    public static <T> StreamCodec<ByteBuf, TagKey<T>> streamCodec(ResourceKey<? extends Registry<T>> registryName) {
        return Identifier.STREAM_CODEC.map(name -> create(registryName, name), TagKey::location);
    }
    public <E> Optional<TagKey<E>> cast(ResourceKey<? extends Registry<E>> registry) {
        return isFor(registry) ? Optional.of((TagKey<E>) this) : Optional.empty();
    }
}
