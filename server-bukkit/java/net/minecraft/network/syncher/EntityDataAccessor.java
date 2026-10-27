package net.minecraft.network.syncher;
public record EntityDataAccessor<T>(int id, EntityDataSerializer<T> serializer) {
    public EntityDataAccessor {
        if (id < 0 || id > 254)
            throw new IllegalArgumentException("Invalid metadata index");
        java.util.Objects.requireNonNull(serializer);
    }
    public int getId() {
        return id;
    }
}
