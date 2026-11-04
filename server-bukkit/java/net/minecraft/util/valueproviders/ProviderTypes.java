// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.valueproviders;

import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.function.Function;
import java.util.function.Supplier;
import net.minecraft.resources.Identifier;

final class ProviderTypes {
    private ProviderTypes() {}

    static <T, K, C extends T> Codec<T> codec(Codec<K> constantType, Function<K, T> constantOf, Class<C> constantClass,
        Function<C, K> constantValue, Map<String, Supplier<MapCodec<? extends T>>> types, Function<T, MapCodec<? extends T>> codecOf) {
        Supplier<Map<Identifier, MapCodec<? extends T>>> table = new Supplier<>() {
            private Map<Identifier, MapCodec<? extends T>> resolved;

            public synchronized Map<Identifier, MapCodec<? extends T>> get() {
                if (resolved == null) {
                    Map<Identifier, MapCodec<? extends T>> byId = new LinkedHashMap<>();
                    types.forEach((name, codec) -> byId.put(Identifier.withDefaultNamespace(name), codec.get()));
                    resolved = byId;
                }
                return resolved;
            }
        };
        Codec<T> dispatch = Identifier.CODEC.dispatch("type", value -> {
            MapCodec<? extends T> own = codecOf.apply(value);
            for (Map.Entry<Identifier, MapCodec<? extends T>> entry : table.get().entrySet()) {
                if (entry.getValue() == own) {
                    return entry.getKey();
                }
            }
            throw new IllegalStateException("Unregistered value provider " + value);
        }, id -> {
            MapCodec<? extends T> found = table.get().get(id);
            if (found == null) {
                throw new IllegalArgumentException("Unknown value provider type " + id);
            }
            return found;
        });
        return Codec.either(constantType, dispatch).xmap(either -> either.map(constantOf, value -> value),
            value -> constantClass.isInstance(value) ? Either.left(constantValue.apply(constantClass.cast(value))) : Either.right(value));
    }
}
