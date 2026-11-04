// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component;

import java.util.stream.Stream;
public interface DataComponentHolder extends DataComponentGetter {
    DataComponentMap getComponents();
    default <T> T get(DataComponentType<? extends T> type) {
        return getComponents().get(type);
    }
    default <T> Stream<T> getAllOfType(Class<? extends T> valueClass) {
        return getComponents().stream().map(TypedDataComponent::value).filter(valueClass::isInstance).map(valueClass::cast);
    }
    default <T> T getOrDefault(DataComponentType<? extends T> type, T defaultValue) {
        T value = get(type);
        return value != null ? value : defaultValue;
    }
    default boolean has(DataComponentType<?> type) {
        return get(type) != null;
    }
}
