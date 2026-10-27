package net.minecraft.network.syncher;
public interface EntityDataSerializer<T> {
    int id();
    void write(io.netty.buffer.ByteBuf out, T value);
    T read(io.netty.buffer.ByteBuf in);
    default EntityDataAccessor<T> createAccessor(int id) {
        return new EntityDataAccessor<>(id, this);
    }
}
