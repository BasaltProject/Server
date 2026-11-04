// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.predicates.DataComponentPredicate;
public interface SingleComponentItemPredicate<T> extends DataComponentPredicate {
    default boolean matches(DataComponentGetter components) {
        T value = components.get(componentType());
        return value != null && matches(value);
    }
    DataComponentType<T> componentType();
    boolean matches(T value);
}
