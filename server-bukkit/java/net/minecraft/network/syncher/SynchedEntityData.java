// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.syncher;
import java.util.*;
import java.util.List;
import net.minecraft.network.RegistryFriendlyByteBuf;
public class SynchedEntityData {
    private final net.minecraft.world.entity.Entity owner;
    private static final Map<Class<?>, Integer> lastIds = new java.util.HashMap<>();
    static {
        lastIds.put(net.minecraft.world.entity.Entity.class, 7);
        lastIds.put(net.minecraft.world.entity.LivingEntity.class, 14);
    }
    private final Map<Integer, DataValue<?>> values = new LinkedHashMap<>();
    private final Set<Integer> dirty = new LinkedHashSet<>();
    public SynchedEntityData(net.minecraft.world.entity.Entity owner) {
        this.owner = owner;
    }
    @SuppressWarnings("unchecked")
    public <T> T get(EntityDataAccessor<T> key) {
        if (key.id() == 0 && key.serializer() == EntityDataSerializers.BYTE && owner != null)
            return (T) Byte.valueOf(owner.sharedFlags());
        DataValue<?> value = values.get(key.id());
        if (value == null)
            throw new IllegalStateException("Undefined metadata index: " + key.id());
        return (T) value.value();
    }
    public <T> void set(EntityDataAccessor<T> key, T value) {
        set(key, value, false);
    }
    public <T> void set(EntityDataAccessor<T> key, T value, boolean force) {
        DataValue<?> old = values.put(key.id(), DataValue.create(key, value));
        if (force || old == null || !Objects.equals(old.value(), value))
            dirty.add(key.id());
        if (key.id() == 0 && key.serializer() == EntityDataSerializers.BYTE && owner != null)
            owner.sharedFlags((Byte) value);
    }
    public boolean isDirty() {
        return !dirty.isEmpty();
    }
    public List<DataValue<?>> packDirty() {
        if (dirty.isEmpty())
            return null;
        List<DataValue<?>> out = new ArrayList<>();
        for (int id : dirty) out.add(values.get(id));
        dirty.clear();
        return out;
    }
    public List<DataValue<?>> getNonDefaultValues() {
        return values.isEmpty() ? null : List.copyOf(values.values());
    }
    public record DataValue<T>(int id, EntityDataSerializer<T> serializer, T value) {
        public int id() { return id; }

        public static <T> DataValue<T> create(EntityDataAccessor<T> key, T value) {
            return new DataValue<>(key.id(), key.serializer(), value);
        }
        public void write(io.netty.buffer.ByteBuf out) {
            out.writeByte(id);
            org.basaltmc.bukkit.net.PacketCodec.varInt(out, serializer.id());
            serializer.write(out, value);
        }
            public void write(RegistryFriendlyByteBuf output) {
            write((io.netty.buffer.ByteBuf) output);
        }
        public static DataValue<?> read(RegistryFriendlyByteBuf input, int id) {
            int serializerId = org.basaltmc.bukkit.net.PacketCodec.readVarInt(input);
            EntityDataSerializer<?> serializer = EntityDataSerializers.getSerializer(serializerId);
            if (serializer == null) {
                throw new IllegalStateException("Unknown serializer type " + serializerId);
            }
            return read(input, id, serializer);
        }
        private static <T> DataValue<T> read(RegistryFriendlyByteBuf input, int id, EntityDataSerializer<T> serializer) {
            return new DataValue<>(id, serializer, serializer.read(input));
        }
}
    private SynchedEntityData(SyncedDataHolder entity, DataItem<?>[] itemsById) {
        this(entity instanceof net.minecraft.world.entity.Entity owner ? owner : null);
        for (DataItem<?> item : itemsById) {
            if (item != null) {
                values.put(item.getAccessor().id(), item.value());
            }
        }
    }
    public static <T> EntityDataAccessor<T> defineId(Class<? extends SyncedDataHolder> clazz, EntityDataSerializer<T> type) {
        synchronized (lastIds) {
            Integer last = lastIds.get(clazz);
            Class<?> parent = clazz.getSuperclass();
            while (last == null && parent != null) {
                last = lastIds.get(parent);
                parent = parent.getSuperclass();
            }
            int assigned = last == null ? 0 : last + 1;
            if (assigned > 254) {
                throw new IllegalArgumentException("Data value id is too big with " + assigned + "! (Max is 254)");
            }
            lastIds.put(clazz, assigned);
            return type.createAccessor(assigned);
        }

    }
    public void assignValues(List<DataValue<?>> items) {
        for (DataValue<?> item : items) {
            values.put(item.id(), item);
        }
        if (owner != null) {
            owner.onSyncedDataUpdated(items);
        }
    }
    public static class DataItem<T> {
        private final EntityDataAccessor<T> accessor;
        private final T initialValue;
        private T value;
        private boolean dirty;
        public DataItem(EntityDataAccessor<T> accessor, T initialValue) {
            this.accessor = accessor;
            this.initialValue = initialValue;
            this.value = initialValue;
        }
        public EntityDataAccessor<T> getAccessor() {
            return accessor;
        }
        public void setValue(T value) {
            this.value = value;
        }
        public T getValue() {
            return value;
        }
        public boolean isDirty() {
            return dirty;
        }
        public void setDirty(boolean dirty) {
            this.dirty = dirty;
        }
        public boolean isSetToDefault() {
            return java.util.Objects.equals(initialValue, value);
        }
        public DataValue<T> value() {
            return DataValue.create(accessor, value);
        }
    }
    public static class Builder {
        private final SyncedDataHolder entity;
        private final java.util.TreeMap<Integer, DataItem<?>> items = new java.util.TreeMap<>();
        public Builder(SyncedDataHolder entity) {
            this.entity = entity;
        }
        public <T> Builder define(EntityDataAccessor<T> accessor, T value) {
            int id = accessor.id();
            if (items.containsKey(id)) {
                throw new IllegalArgumentException("Duplicate id value for " + id + "!");
            }
            items.put(id, new DataItem<>(accessor, value));
            return this;
        }
        public SynchedEntityData build() {
            SynchedEntityData data = new SynchedEntityData(entity, items.values().toArray(new DataItem<?>[0]));
            return data;
        }
    }
}
