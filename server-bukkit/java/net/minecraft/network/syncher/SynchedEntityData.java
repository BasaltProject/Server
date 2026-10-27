package net.minecraft.network.syncher;
import java.util.*;
public class SynchedEntityData {
    private final net.minecraft.world.entity.Entity owner;
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
    }
}
