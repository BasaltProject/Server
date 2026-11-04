// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.memory;

import com.google.common.collect.ImmutableMap;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import java.util.Iterator;
import java.util.Map;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import net.minecraft.core.registries.BuiltInRegistries;

public final class MemoryMap implements Iterable<MemoryMap.Value<?>> {
    private static final Codec<MemoryModuleType<?>> SERIALIZABLE_TYPE = Codec.lazyInitialized(() -> BuiltInRegistries.MEMORY_MODULE_TYPE
        .byNameCodec().validate(type -> type.canSerialize() ? DataResult.success(type)
            : DataResult.error(() -> "Memory module " + type + " cannot be encoded")));
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static final Codec<MemoryMap> CODEC = Codec.lazyInitialized(() -> ((Codec<Map>) (Codec) Codec.dispatchedMap(SERIALIZABLE_TYPE,
        type -> type.getCodec().orElseThrow())).xmap(map -> new MemoryMap(map), map -> map.memories));
    public static final MemoryMap EMPTY = new MemoryMap(Map.of());

    private final Map<MemoryModuleType<?>, ExpirableValue<?>> memories;

    private MemoryMap(Map<MemoryModuleType<?>, ExpirableValue<?>> memories) {
        this.memories = Map.copyOf(memories);
    }

    public static MemoryMap of(Stream<Value<?>> memories) {
        return new MemoryMap(memories.collect(Collectors.toMap(Value::type, Value::value)));
    }

    @SuppressWarnings("unchecked")
    public <U> ExpirableValue<U> get(MemoryModuleType<U> type) {
        return (ExpirableValue<U>) memories.get(type);
    }

    @Override
    public boolean equals(Object obj) {
        return obj instanceof MemoryMap other && memories.equals(other.memories);
    }

    @Override
    public int hashCode() {
        return memories.hashCode();
    }

    @Override
    public String toString() {
        return memories.toString();
    }

    @Override
    public Iterator<Value<?>> iterator() {
        Iterator<? extends Map.Entry<MemoryModuleType<?>, ExpirableValue<?>>> entries = memories.entrySet().iterator();
        return new Iterator<>() {
            @Override
            public boolean hasNext() {
                return entries.hasNext();
            }

            @Override
            public Value<?> next() {
                Map.Entry<MemoryModuleType<?>, ExpirableValue<?>> entry = entries.next();
                return Value.createUnchecked(entry.getKey(), entry.getValue());
            }
        };
    }

    public static record Value<U>(MemoryModuleType<U> type, ExpirableValue<U> value) {
        @SuppressWarnings("unchecked")
        public static <U> Value<U> createUnchecked(MemoryModuleType<U> type, ExpirableValue<?> value) {
            return new Value<>(type, (ExpirableValue<U>) value);
        }
    }

    public static class Builder {
        private final ImmutableMap.Builder<MemoryModuleType<?>, ExpirableValue<?>> builder = ImmutableMap.builder();

        public <U> Builder add(MemoryModuleType<U> type, ExpirableValue<U> value) {
            builder.put(type, value);
            return this;
        }

        public MemoryMap build() {
            return new MemoryMap(builder.buildOrThrow());
        }
    }
}
