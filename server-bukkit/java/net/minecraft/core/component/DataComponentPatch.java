// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component;

import com.mojang.serialization.Codec;
import com.mojang.serialization.Dynamic;
import com.mojang.serialization.JavaOps;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.function.Predicate;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;

/** Raw serialized changes (kept as written by whoever decoded them) plus typed changes made through the API. */
public final class DataComponentPatch {
    public static final DataComponentPatch EMPTY = new DataComponentPatch(null, Map.of());
    public static final Codec<DataComponentPatch> CODEC = Codec.PASSTHROUGH.xmap(
        data -> new DataComponentPatch(data, Map.of()), DataComponentPatch::encoded);
    public static final StreamCodec<RegistryFriendlyByteBuf, DataComponentPatch> STREAM_CODEC = StreamCodec.of(DataComponentPatch::write, DataComponentPatch::read);
    public static final StreamCodec<RegistryFriendlyByteBuf, DataComponentPatch> DELIMITED_STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.core.component.DataComponentPatch.class, "DELIMITED_STREAM_CODEC");

    private final Dynamic<?> data;
    private final Map<DataComponentType<?>, Optional<?>> typed;
    private final java.util.concurrent.ConcurrentHashMap<DataComponentType<?>, Optional<?>> decoded = new java.util.concurrent.ConcurrentHashMap<>();

    private DataComponentPatch(Dynamic<?> data, Map<DataComponentType<?>, Optional<?>> typed) {
        this.data = data;
        this.typed = typed;
    }

    private static String idOf(DataComponentType<?> type, boolean removed) {
        String text = type.toString();
        String qualified = text.contains(":") ? text : "minecraft:" + text;
        return removed ? "!" + qualified : qualified;
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static Dynamic<?> encoded(DataComponentPatch patch) {
        Dynamic<?> base = patch.data != null ? patch.data : new Dynamic<>(JavaOps.INSTANCE, JavaOps.INSTANCE.emptyMap());
        for (Map.Entry<DataComponentType<?>, Optional<?>> entry : patch.typed.entrySet()) {
            DataComponentType type = entry.getKey();
            base = base.remove(idOf(type, entry.getValue().isPresent()));
            if (entry.getValue().isEmpty()) {
                base = base.remove(idOf(type, false));
                base = base.set(idOf(type, true), base.emptyMap());
            } else if (type.codec() != null) {
                com.mojang.serialization.DynamicOps ops = base.getOps();
                Object encodedValue = ((Codec) type.codec()).encodeStart(ops, entry.getValue().get()).result()
                    .orElse(null);
                if (encodedValue != null) {
                    base = base.set(idOf(type, false), new Dynamic(ops, encodedValue));
                }
            }
        }
        return base;
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static void write(RegistryFriendlyByteBuf buf, DataComponentPatch patch) {
        java.util.List<DataComponentType<?>> removed = new java.util.ArrayList<>();
        java.util.Map<DataComponentType<?>, Object> added = new LinkedHashMap<>();
        patch.forEachChange((type, value) -> {
            if (value.isPresent()) {
                added.put(type, value.get());
            } else {
                removed.add(type);
            }
        });
        buf.writeVarInt(added.size());
        buf.writeVarInt(removed.size());
        for (Map.Entry<DataComponentType<?>, Object> entry : added.entrySet()) {
            DataComponentType type = entry.getKey();
            if (type.streamCodec() == null) {
                throw new UnsupportedOperationException("Component " + type + " has no network codec in the Basalt facade");
            }
            buf.writeVarInt(DataComponents.registry().getId(type));
            type.streamCodec().encode(buf, entry.getValue());
        }
        for (DataComponentType<?> type : removed) {
            buf.writeVarInt(DataComponents.registry().getId(type));
        }
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static DataComponentPatch read(RegistryFriendlyByteBuf buf) {
        int added = buf.readVarInt();
        int removed = buf.readVarInt();
        Builder patch = builder();
        for (int i = 0; i < added; i++) {
            DataComponentType type = DataComponents.registry().byId(buf.readVarInt());
            if (type == null || type.streamCodec() == null) {
                throw new UnsupportedOperationException("Component " + type + " has no network codec in the Basalt facade");
            }
            patch.set(type, type.streamCodec().decode(buf));
        }
        for (int i = 0; i < removed; i++) {
            patch.remove(DataComponents.registry().byId(buf.readVarInt()));
        }
        return patch.build();
    }

    public boolean isEmpty() {
        return data == null && typed.isEmpty();
    }

    /** Visits every change, raw ones included; a raw change whose type has no value codec cannot be decoded and throws. */
    @SuppressWarnings({"unchecked", "rawtypes"})
    public void forEachChange(java.util.function.BiConsumer<DataComponentType<?>, Optional<?>> visitor) {
        typed.forEach(visitor);
        if (data == null) {
            return;
        }
        for (Map.Entry<? extends Dynamic<?>, ? extends Dynamic<?>> entry : data.getMapValues().result().orElse(Map.of()).entrySet()) {
            String key = entry.getKey().asString("");
            boolean removed = key.startsWith("!");
            net.minecraft.resources.Identifier id = net.minecraft.resources.Identifier.parse(removed ? key.substring(1) : key);
            DataComponentType type = net.minecraft.core.component.DataComponents.registry().getValue(id);
            if (type == null || typed.containsKey(type)) {
                continue;
            }
            if (removed) {
                visitor.accept(type, Optional.empty());
            } else if (type.codec() == null) {
                throw new UnsupportedOperationException("Component " + id + " has no value codec in the Basalt facade");
            } else {
                visitor.accept(type, ((Codec) type.codec()).parse(data.getOps(), entry.getValue().getValue()).result());
            }
        }
    }

    public Dynamic<?> data() {
        return data;
    }

    public static Builder builder() {
        return new Builder();
    }

    @SuppressWarnings("unchecked")
    public <T> T get(DataComponentGetter prototype, DataComponentType<? extends T> type) {
        Optional<?> changed = typed.get(type);
        if (changed != null) {
            return (T) changed.orElse(null);
        }
        if (data != null && type.codec() != null) {
            Optional<?> raw = decoded.computeIfAbsent(type, key -> rawValue(key));
            if (raw.isPresent()) {
                return (T) raw.get();
            }
            if (data.get(idOf(type, true)).result().isPresent()) {
                return null;
            }
        }
        return prototype.get(type);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private Optional<?> rawValue(DataComponentType<?> type) {
        Optional<? extends Dynamic<?>> stored = data.get(idOf(type, false)).result();
        if (stored.isEmpty()) {
            return Optional.empty();
        }
        return ((Codec) type.codec()).parse(data.getOps(), stored.get().getValue()).result();
    }

    public int size() {
        int raw = data == null ? 0 : (int) data.getMapValues().result().map(map -> map.size()).orElse(0);
        return typed.size() + raw;
    }

    public DataComponentPatch forget(Predicate<DataComponentType<?>> test) {
        if (typed.isEmpty()) {
            return this;
        }
        Map<DataComponentType<?>, Optional<?>> kept = new LinkedHashMap<>();
        for (Map.Entry<DataComponentType<?>, Optional<?>> entry : typed.entrySet()) {
            if (!test.test(entry.getKey())) {
                kept.put(entry.getKey(), entry.getValue());
            }
        }
        return new DataComponentPatch(data, kept);
    }

    public SplitResult split() {
        if (typed.isEmpty()) {
            return SplitResult.EMPTY;
        }
        DataComponentMap.Builder added = DataComponentMap.builder();
        Set<DataComponentType<?>> removed = new LinkedHashSet<>();
        for (Map.Entry<DataComponentType<?>, Optional<?>> entry : typed.entrySet()) {
            if (entry.getValue().isPresent()) {
                added.setUnchecked(entry.getKey(), entry.getValue().get());
            } else {
                removed.add(entry.getKey());
            }
        }
        return new SplitResult(added.build(), removed);
    }

    static DataComponentPatch merge(DataComponentPatch rawSource, Map<DataComponentType<?>, Optional<?>> typed) {
        return rawSource.data == null && typed.isEmpty() ? EMPTY
            : new DataComponentPatch(rawSource.data, new LinkedHashMap<>(typed));
    }

    Map<DataComponentType<?>, Optional<?>> typedChanges() {
        return typed;
    }

    @Override
    public boolean equals(Object obj) {
        return obj instanceof DataComponentPatch other && java.util.Objects.equals(data, other.data)
            && typed.equals(other.typed);
    }

    @Override
    public int hashCode() {
        return 31 * java.util.Objects.hashCode(data) + typed.hashCode();
    }

    @Override
    public String toString() {
        return data == null ? typed.toString() : String.valueOf(data.getValue()) + typed;
    }

    public static record SplitResult(DataComponentMap added, Set<DataComponentType<?>> removed) {
        public static final SplitResult EMPTY = new SplitResult(DataComponentMap.EMPTY, Set.of());
    }

    public static class Builder {
        private final Map<DataComponentType<?>, Optional<?>> map = new LinkedHashMap<>();

        private Builder() {
        }

        public <T> Builder set(DataComponentType<T> type, T value) {
            map.put(type, Optional.of(value));
            return this;
        }

        public <T> Builder remove(DataComponentType<T> type) {
            map.put(type, Optional.empty());
            return this;
        }

        public <T> Builder set(TypedDataComponent<T> component) {
            return set(component.type(), component.value());
        }

        public <T> Builder set(Iterable<TypedDataComponent<?>> components) {
            for (TypedDataComponent<?> component : components) {
                map.put(component.type(), Optional.of(component.value()));
            }
            return this;
        }

        public DataComponentPatch build() {
            return map.isEmpty() ? EMPTY : new DataComponentPatch(null, new LinkedHashMap<>(map));
        }
    }
}
