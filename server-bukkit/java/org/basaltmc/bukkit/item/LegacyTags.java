package org.basaltmc.bukkit.item;

import org.bukkit.NamespacedKey;
import org.bukkit.inventory.meta.tags.CustomItemTagContainer;
import org.bukkit.inventory.meta.tags.ItemTagAdapterContext;
import org.bukkit.inventory.meta.tags.ItemTagType;
import org.bukkit.persistence.PersistentDataContainer;
import org.bukkit.persistence.PersistentDataType;

/** The deprecated item tag container over the persistent data container the stack already keeps. */
@SuppressWarnings({"deprecation", "removal"})
final class LegacyTags implements CustomItemTagContainer {
    private final PersistentDataContainer container;
    private final ItemTagAdapterContext context;

    LegacyTags(PersistentDataContainer container) {
        this.container = container;
        this.context = () -> new LegacyTags(container.getAdapterContext().newPersistentDataContainer());
    }

    @SuppressWarnings("unchecked")
    private static <T> PersistentDataType<T, T> primitive(Class<T> type) {
        if (type == Byte.class) {
            return (PersistentDataType<T, T>) PersistentDataType.BYTE;
        } else if (type == Short.class) {
            return (PersistentDataType<T, T>) PersistentDataType.SHORT;
        } else if (type == Integer.class) {
            return (PersistentDataType<T, T>) PersistentDataType.INTEGER;
        } else if (type == Long.class) {
            return (PersistentDataType<T, T>) PersistentDataType.LONG;
        } else if (type == Float.class) {
            return (PersistentDataType<T, T>) PersistentDataType.FLOAT;
        } else if (type == Double.class) {
            return (PersistentDataType<T, T>) PersistentDataType.DOUBLE;
        } else if (type == String.class) {
            return (PersistentDataType<T, T>) PersistentDataType.STRING;
        } else if (type == byte[].class) {
            return (PersistentDataType<T, T>) PersistentDataType.BYTE_ARRAY;
        } else if (type == int[].class) {
            return (PersistentDataType<T, T>) PersistentDataType.INTEGER_ARRAY;
        } else if (type == long[].class) {
            return (PersistentDataType<T, T>) PersistentDataType.LONG_ARRAY;
        } else if (type == CustomItemTagContainer.class) {
            return (PersistentDataType<T, T>) PersistentDataType.TAG_CONTAINER;
        }
        throw new IllegalArgumentException("Unsupported tag type " + type.getName());
    }

    @Override
    @SuppressWarnings("unchecked")
    public <T, Z> void setCustomTag(NamespacedKey key, ItemTagType<T, Z> type, Z value) {
        T stored = type.toPrimitive(value, context);
        if (stored instanceof LegacyTags nested) {
            container.set(key, PersistentDataType.TAG_CONTAINER, nested.container);
        } else {
            container.set(key, primitive(type.getPrimitiveType()), stored);
        }
    }

    @Override
    @SuppressWarnings("unchecked")
    public <T, Z> boolean hasCustomTag(NamespacedKey key, ItemTagType<T, Z> type) {
        if (type.getPrimitiveType() == CustomItemTagContainer.class) {
            return container.has(key, PersistentDataType.TAG_CONTAINER);
        }
        return container.has(key, primitive(type.getPrimitiveType()));
    }

    @Override
    @SuppressWarnings("unchecked")
    public <T, Z> Z getCustomTag(NamespacedKey key, ItemTagType<T, Z> type) {
        if (type.getPrimitiveType() == CustomItemTagContainer.class) {
            PersistentDataContainer nested = container.get(key, PersistentDataType.TAG_CONTAINER);
            return nested == null ? null : type.fromPrimitive((T) new LegacyTags(nested), context);
        }
        T stored = container.get(key, primitive(type.getPrimitiveType()));
        return stored == null ? null : type.fromPrimitive(stored, context);
    }

    @Override
    public void removeCustomTag(NamespacedKey key) {
        container.remove(key);
    }

    @Override
    public boolean isEmpty() {
        return container.isEmpty();
    }

    @Override
    public ItemTagAdapterContext getAdapterContext() {
        return context;
    }
}
