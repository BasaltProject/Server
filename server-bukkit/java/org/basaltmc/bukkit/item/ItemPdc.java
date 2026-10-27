package org.basaltmc.bukkit.item;

import java.io.IOException;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import org.bukkit.NamespacedKey;
import org.bukkit.persistence.PersistentDataAdapterContext;
import org.bukkit.persistence.PersistentDataContainer;
import org.bukkit.persistence.PersistentDataType;

public final class ItemPdc implements PersistentDataContainer, PersistentDataAdapterContext {
    private final Map<String, Object> components;
    private final Map<String, Object> nested;
    private final Runnable changed;

    public ItemPdc(Map<String, Object> components) {
        this(components, null);
    }

    public ItemPdc(Map<String, Object> components, Runnable changed) {
        this.components = components;
        this.nested = null;
        this.changed = changed;
    }

    private ItemPdc(Map<String, Object> nested, boolean unused) {
        this.components = null;
        this.nested = nested;
        this.changed = null;
    }

    private void touched() {
        if (changed != null) {
            changed.run();
        }
    }

    private Map<String, Object> values(boolean create) {
        if (nested != null) {
            return nested;
        }
        Map<String, Object> data = Nbt.map(components.get(MetaItem.kCustomData));
        if (data == null) {
            if (!create) {
                return null;
            }
            data = Nbt.compound();
            components.put(MetaItem.kCustomData, data);
        }
        Map<String, Object> values = Nbt.map(data.get("PublicBukkitValues"));
        if (values == null && create) {
            values = Nbt.compound();
            data.put("PublicBukkitValues", values);
        }
        return values;
    }

    private void tidy() {
        if (nested != null) {
            return;
        }
        Map<String, Object> data = Nbt.map(components.get(MetaItem.kCustomData));
        if (data == null) {
            return;
        }
        Map<String, Object> values = Nbt.map(data.get("PublicBukkitValues"));
        if (values != null && values.isEmpty()) {
            data.remove("PublicBukkitValues");
        }
        if (data.isEmpty()) {
            components.remove(MetaItem.kCustomData);
        }
    }

    private static Object store(Object primitive, Class<?> kind) {
        if (primitive instanceof PersistentDataContainer container) {
            if (!(container instanceof ItemPdc own)) {
                try {
                    Map<String, Object> read = Nbt.read(container.serializeToBytes());
                    return read == null ? Nbt.compound() : read;
                } catch (IOException failure) {
                    throw new IllegalArgumentException("Could not read the container", failure);
                }
            }
            Map<String, Object> inner = own.values(true);
            return Nbt.deepCopy(inner == null ? Nbt.compound() : inner);
        }
        if (primitive instanceof PersistentDataContainer[] containers) {
            List<Object> out = new ArrayList<>();
            for (PersistentDataContainer container : containers) {
                out.add(store(container, PersistentDataContainer.class));
            }
            return out;
        }
        if (primitive instanceof List<?> list) {
            List<Object> out = new ArrayList<>();
            for (Object element : list) {
                out.add(store(element, element.getClass()));
            }
            return out;
        }
        return Nbt.copy(primitive);
    }

    private Object load(Object stored, Class<?> kind) {
        if (kind == PersistentDataContainer.class) {
            Map<String, Object> map = Nbt.map(stored);
            return map == null ? null : new ItemPdc(Nbt.deepCopy(map), true);
        }
        if (kind == PersistentDataContainer[].class) {
            List<Object> list = Nbt.list(stored);
            if (list == null) {
                return null;
            }
            PersistentDataContainer[] out = new PersistentDataContainer[list.size()];
            for (int at = 0; at < out.length; at++) {
                out[at] = (PersistentDataContainer) load(list.get(at), PersistentDataContainer.class);
            }
            return out;
        }
        if (kind == List.class) {
            List<Object> list = Nbt.list(stored);
            if (list == null) {
                return null;
            }
            List<Object> out = new ArrayList<>();
            for (Object element : list) {
                out.add(element instanceof Map ? load(element, PersistentDataContainer.class) : element);
            }
            return out;
        }
        if (kind == Byte.class && stored instanceof Number number) {
            return number.byteValue();
        }
        if (kind == Short.class && stored instanceof Number number) {
            return number.shortValue();
        }
        if (kind == Integer.class && stored instanceof Number number) {
            return number.intValue();
        }
        if (kind == Long.class && stored instanceof Number number) {
            return number.longValue();
        }
        if (kind == Float.class && stored instanceof Number number) {
            return number.floatValue();
        }
        if (kind == Double.class && stored instanceof Number number) {
            return number.doubleValue();
        }
        return kind.isInstance(stored) ? stored : null;
    }

    private static boolean matches(Object stored, Class<?> kind) {
        if (kind == PersistentDataContainer.class) {
            return stored instanceof Map;
        }
        if (kind == PersistentDataContainer[].class || kind == List.class) {
            return stored instanceof List;
        }
        if (kind == Byte.class || kind == Short.class || kind == Integer.class || kind == Long.class
            || kind == Float.class || kind == Double.class) {
            return stored instanceof Number && stored.getClass() == kind;
        }
        return kind.isInstance(stored);
    }

    @Override
    public <P, C> void set(NamespacedKey key, PersistentDataType<P, C> type, C value) {
        if (key == null || type == null || value == null) {
            throw new IllegalArgumentException("key, type and value must not be null");
        }
        P primitive = type.toPrimitive(value, this);
        values(true).put(key.toString(), store(primitive, type.getPrimitiveType()));
        touched();
    }

    @Override
    public <P, C> boolean has(NamespacedKey key, PersistentDataType<P, C> type) {
        Map<String, Object> values = values(false);
        Object stored = values == null ? null : values.get(key.toString());
        return stored != null && matches(stored, type.getPrimitiveType());
    }

    @Override
    public boolean has(NamespacedKey key) {
        Map<String, Object> values = values(false);
        return values != null && values.containsKey(key.toString());
    }

    @Override
    @SuppressWarnings("unchecked")
    public <P, C> C get(NamespacedKey key, PersistentDataType<P, C> type) {
        Map<String, Object> values = values(false);
        Object stored = values == null ? null : values.get(key.toString());
        if (stored == null) {
            return null;
        }
        if (!matches(stored, type.getPrimitiveType())) {
            throw new IllegalArgumentException("The found object is of the type " + stored.getClass().getSimpleName()
                + ", not " + type.getPrimitiveType().getSimpleName());
        }
        Object primitive = load(stored, type.getPrimitiveType());
        return primitive == null ? null : type.fromPrimitive((P) primitive, this);
    }

    @Override
    public <P, C> C getOrDefault(NamespacedKey key, PersistentDataType<P, C> type, C defaultValue) {
        C found = get(key, type);
        return found != null ? found : defaultValue;
    }

    @Override
    public Set<NamespacedKey> getKeys() {
        Set<NamespacedKey> out = new LinkedHashSet<>();
        Map<String, Object> values = values(false);
        if (values != null) {
            for (String name : values.keySet()) {
                NamespacedKey key = NamespacedKey.fromString(name);
                if (key != null) {
                    out.add(key);
                }
            }
        }
        return out;
    }

    @Override
    public boolean isEmpty() {
        Map<String, Object> values = values(false);
        return values == null || values.isEmpty();
    }

    @Override
    public int getSize() {
        Map<String, Object> values = values(false);
        return values == null ? 0 : values.size();
    }

    @Override
    public void remove(NamespacedKey key) {
        Map<String, Object> values = values(false);
        if (values != null) {
            values.remove(key.toString());
            tidy();
            touched();
        }
    }

    @Override
    public void copyTo(PersistentDataContainer other, boolean replace) {
        Map<String, Object> values = values(false);
        if (values == null) {
            return;
        }
        if (!(other instanceof ItemPdc target)) {
            copyForeign(other, values, replace);
            return;
        }
        Map<String, Object> into = target.values(true);
        for (Map.Entry<String, Object> entry : values.entrySet()) {
            if (replace || !into.containsKey(entry.getKey())) {
                into.put(entry.getKey(), Nbt.copy(entry.getValue()));
            }
        }
        target.touched();
    }

    private static void copyForeign(PersistentDataContainer other, Map<String, Object> values, boolean replace) {
        java.util.Set<String> present = new java.util.HashSet<>();
        for (org.bukkit.NamespacedKey key : other.getKeys()) {
            present.add(key.toString());
        }
        Map<String, Object> chosen = Nbt.compound();
        for (Map.Entry<String, Object> entry : values.entrySet()) {
            if (replace || !present.contains(entry.getKey())) {
                chosen.put(entry.getKey(), Nbt.copy(entry.getValue()));
            }
        }
        try {
            other.readFromBytes(Nbt.write(chosen), false);
        } catch (IOException failure) {
            throw new IllegalArgumentException("Could not copy the container", failure);
        }
    }

    @Override
    public PersistentDataAdapterContext getAdapterContext() {
        return this;
    }

    @Override
    public PersistentDataContainer newPersistentDataContainer() {
        return new ItemPdc(Nbt.compound(), true);
    }

    @Override
    public byte[] serializeToBytes() throws IOException {
        Map<String, Object> values = values(false);
        return Nbt.write(values == null ? Nbt.compound() : values);
    }

    @Override
    public void readFromBytes(byte[] bytes, boolean clear) throws IOException {
        Map<String, Object> read = Nbt.read(bytes);
        if (read == null) {
            throw new IOException("not a compound");
        }
        Map<String, Object> values = values(true);
        if (clear) {
            values.clear();
        }
        values.putAll(read);
        tidy();
        touched();
    }
}
