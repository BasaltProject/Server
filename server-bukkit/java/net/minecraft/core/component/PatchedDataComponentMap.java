// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component;

import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Optional;
import java.util.Set;

public final class PatchedDataComponentMap implements DataComponentMap {
    private final DataComponentMap prototype;
    private final Map<DataComponentType<?>, Optional<?>> patch = new LinkedHashMap<>();
    private DataComponentPatch raw = DataComponentPatch.EMPTY;

    public PatchedDataComponentMap(DataComponentMap prototype) {
        this.prototype = prototype;
    }

    public static PatchedDataComponentMap fromPatch(DataComponentMap prototype, DataComponentPatch patch) {
        PatchedDataComponentMap map = new PatchedDataComponentMap(prototype);
        map.applyPatch(patch);
        return map;
    }

    @Override
    @SuppressWarnings("unchecked")
    public <T> T get(DataComponentType<? extends T> type) {
        Optional<?> changed = patch.get(type);
        return changed != null ? (T) changed.orElse(null) : prototype.get(type);
    }

    public boolean hasNonDefault(DataComponentType<?> type) {
        return patch.containsKey(type);
    }

    public <T> T set(DataComponentType<T> type, T value) {
        T previous = get(type);
        patch.put(type, Optional.of(value));
        return previous;
    }

    public <T> T set(TypedDataComponent<T> value) {
        return set(value.type(), value.value());
    }

    public <T> T remove(DataComponentType<? extends T> type) {
        T previous = get(type);
        patch.put(type, Optional.empty());
        return previous;
    }

    public void applyPatch(DataComponentPatch changes) {
        patch.putAll(changes.typedChanges());
        if (changes.data() != null) {
            raw = changes;
        }
    }

    public void restorePatch(DataComponentPatch changes) {
        patch.clear();
        applyPatch(changes);
    }

    public void clearPatch() {
        patch.clear();
        raw = DataComponentPatch.EMPTY;
    }

    public void setAll(DataComponentMap components) {
        for (TypedDataComponent<?> entry : components) {
            patch.put(entry.type(), Optional.of(entry.value()));
        }
    }

    @Override
    public Set<DataComponentType<?>> keySet() {
        Set<DataComponentType<?>> keys = new LinkedHashSet<>(prototype.keySet());
        for (Map.Entry<DataComponentType<?>, Optional<?>> entry : patch.entrySet()) {
            if (entry.getValue().isPresent()) {
                keys.add(entry.getKey());
            } else {
                keys.remove(entry.getKey());
            }
        }
        return keys;
    }

    @Override
    public Iterator<TypedDataComponent<?>> iterator() {
        return stream().iterator();
    }

    @Override
    public int size() {
        return keySet().size();
    }

    public DataComponentPatch asPatch() {
        return DataComponentPatch.merge(raw, patch);
    }

    public PatchedDataComponentMap copy() {
        PatchedDataComponentMap copy = new PatchedDataComponentMap(prototype);
        copy.patch.putAll(patch);
        copy.raw = raw;
        return copy;
    }

    public DataComponentMap toImmutableMap() {
        DataComponentMap.Builder builder = DataComponentMap.builder();
        builder.addAll(prototype);
        for (Map.Entry<DataComponentType<?>, Optional<?>> entry : patch.entrySet()) {
            if (entry.getValue().isEmpty()) {
                builder.setUnchecked(entry.getKey(), null);
            } else {
                builder.setUnchecked(entry.getKey(), entry.getValue().get());
            }
        }
        return builder.build();
    }

    @Override
    public boolean equals(Object obj) {
        return obj instanceof PatchedDataComponentMap other && prototype.equals(other.prototype)
            && patch.equals(other.patch) && raw.equals(other.raw);
    }

    @Override
    public int hashCode() {
        return prototype.hashCode() + 31 * patch.hashCode() + raw.hashCode();
    }

    @Override
    public String toString() {
        return "{" + keySet() + "}";
    }
}
