package org.basaltmc.bukkit.facade;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.HashMap;
import java.util.Map;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.DataComponents;
import net.minecraft.resources.Identifier;

/** Name lookup over the declared data component types. */
public final class ComponentTypes {
    private static volatile Map<Identifier, DataComponentType<?>> kById;

    private ComponentTypes() {}

    private static Map<Identifier, DataComponentType<?>> table() {
        Map<Identifier, DataComponentType<?>> found = kById;
        if (found == null) {
            found = new HashMap<>();
            for (Field field : DataComponents.class.getDeclaredFields()) {
                if (field.getType() == DataComponentType.class && Modifier.isStatic(field.getModifiers())) {
                    try {
                        DataComponentType<?> type = (DataComponentType<?>) field.get(null);
                        if (type != null) {
                            found.put(idOf(type), type);
                        }
                    } catch (IllegalAccessException failure) {
                        continue;
                    }
                }
            }
            kById = found;
        }
        return found;
    }

    public static DataComponentType<?> byId(Identifier id) {
        return table().get(id);
    }

    public static Identifier idOf(DataComponentType<?> type) {
        String text = type.toString();
        return Identifier.parse(text.contains(":") ? text : "minecraft:" + text);
    }
}
