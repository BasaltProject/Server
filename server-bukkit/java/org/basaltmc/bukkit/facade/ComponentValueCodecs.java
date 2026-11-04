package org.basaltmc.bukkit.facade;

import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import java.util.LinkedHashMap;
import java.util.Map;
import net.minecraft.core.component.DataComponentType;

/** Codec for a map from component type to its own typed value. */
public final class ComponentValueCodecs {
    private ComponentValueCodecs() {}

    public static Codec<Map<DataComponentType<?>, Object>> valueMap(Codec<DataComponentType<?>> typeCodec) {
        return new Codec<>() {
            @Override
            @SuppressWarnings({"unchecked", "rawtypes"})
            public <T> DataResult<Pair<Map<DataComponentType<?>, Object>, T>> decode(DynamicOps<T> ops, T input) {
                DataResult<java.util.stream.Stream<Pair<T, T>>> entries = ops.getMapValues(input);
                if (entries.error().isPresent()) {
                    return DataResult.error(() -> "Not a map of components");
                }
                Map<DataComponentType<?>, Object> out = new LinkedHashMap<>();
                for (Pair<T, T> entry : (Iterable<Pair<T, T>>) entries.result().get()::iterator) {
                    DataResult<DataComponentType<?>> type = typeCodec.parse(ops, entry.getFirst());
                    if (type.error().isPresent()) {
                        return DataResult.error(() -> "Unknown component type");
                    }
                    Codec value = type.result().get().codec();
                    if (value == null) {
                        return DataResult.error(() -> "Component " + type.result().get() + " has no codec");
                    }
                    DataResult<?> parsed = value.parse(ops, entry.getSecond());
                    if (parsed.error().isPresent()) {
                        return DataResult.error(() -> "Bad value for " + type.result().get());
                    }
                    out.put(type.result().get(), parsed.result().get());
                }
                return DataResult.success(Pair.of(out, input));
            }

            @Override
            @SuppressWarnings({"unchecked", "rawtypes"})
            public <T> DataResult<T> encode(Map<DataComponentType<?>, Object> input, DynamicOps<T> ops, T prefix) {
                Map<T, T> out = new LinkedHashMap<>();
                for (Map.Entry<DataComponentType<?>, Object> entry : input.entrySet()) {
                    Codec value = entry.getKey().codec();
                    if (value == null) {
                        return DataResult.error(() -> "Component " + entry.getKey() + " has no codec");
                    }
                    DataResult<T> key = typeCodec.encodeStart(ops, entry.getKey());
                    DataResult<T> encoded = value.encodeStart(ops, entry.getValue());
                    if (key.error().isPresent() || encoded.error().isPresent()) {
                        return DataResult.error(() -> "Cannot encode " + entry.getKey());
                    }
                    out.put(key.result().get(), encoded.result().get());
                }
                return DataResult.success(ops.createMap(out));
            }
        };
    }
}
