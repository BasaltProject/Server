// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component;
public interface DataComponentGetter {
    default <T> T get(DataComponentType<? extends T> type) {
        return null;
    }
    default <T> T getOrDefault(DataComponentType<? extends T> type, T defaultValue) {
        T value = get(type);
        return value != null ? value : defaultValue;
    }
    default <T> TypedDataComponent<T> getTyped(DataComponentType<T> type) {
        T value = get(type);
        return value == null ? null : new TypedDataComponent<>(type, value);
    }
}
