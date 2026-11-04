package org.basaltmc.bukkit.facade;

import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.MapLike;
import java.util.LinkedHashMap;
import java.util.Map;

/** A `type` keyed union over record classes, each serialised by its reflective record codec. */
public final class DispatchCodecs {
    private DispatchCodecs() {}

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static <T> Codec<T> create(Map<String, Class<? extends T>> types) {
        Map<Class<?>, String> names = new LinkedHashMap<>();
        types.forEach((name, cls) -> names.put(cls, name));
        return new Codec<T>() {
            @Override
            public <O> DataResult<Pair<T, O>> decode(DynamicOps<O> ops, O input) {
                DataResult<MapLike<O>> map = ops.getMap(input);
                if (map.error().isPresent()) {
                    return DataResult.error(() -> "Not a map");
                }
                O type = map.result().get().get("type");
                String name = type == null ? "" : ops.getStringValue(type).result().orElse("");
                Class<? extends T> cls = types.get(name.startsWith("minecraft:") ? name.substring(10) : name);
                Codec inner = cls == null ? null : ReflectiveCodecs.forType(cls);
                if (inner == null) {
                    return DataResult.error(() -> "Unknown type " + name);
                }
                return inner.decode(ops, input);
            }

            @Override
            public <O> DataResult<O> encode(T input, DynamicOps<O> ops, O prefix) {
                String name = names.get(input.getClass());
                Codec inner = name == null ? null : ReflectiveCodecs.forType(input.getClass());
                if (inner == null) {
                    return DataResult.error(() -> "Unregistered " + input.getClass().getSimpleName());
                }
                DataResult<O> encoded = inner.encodeStart(ops, input);
                return encoded.flatMap(value -> ops.mergeToMap(value, ops.createString("type"), ops.createString(name)));
            }
        };
    }
}
