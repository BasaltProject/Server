// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.syncher;

import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
public interface EntityDataSerializer<T> {
    int id();
    void write(io.netty.buffer.ByteBuf out, T value);
    T read(io.netty.buffer.ByteBuf in);
    default EntityDataAccessor<T> createAccessor(int id) {
        return new EntityDataAccessor<>(id, this);
    }
    default StreamCodec<? super RegistryFriendlyByteBuf, T> codec() {
        return StreamCodec.<RegistryFriendlyByteBuf, T>of((buf, value) -> write(buf, value), buf -> read(buf));
    }
    default T copy(T value) {
        return value;
    }
    static <T> EntityDataSerializer<T> forValueType(StreamCodec<? super RegistryFriendlyByteBuf, T> codec) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.syncher.EntityDataSerializer#forValueType");
    }
    public interface ForValueType<T> extends EntityDataSerializer<T> {
        default T copy(T value) {
            return value;
        }
    }
}
