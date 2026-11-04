// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
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
    public boolean equals(Object o) {
        return this == o || (o instanceof EntityDataAccessor<?> other && id == other.id);
    }
    public int hashCode() {
        return id;
    }
    public String toString() {
        return "<entity data: " + id + ">";
    }
}
