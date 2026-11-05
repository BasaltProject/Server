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
        return new EntityDataSerializer<T>() {
            public int id() {
                return -1;
            }

            public void write(io.netty.buffer.ByteBuf out, T value) {
                codec.encode((RegistryFriendlyByteBuf) out, value);
            }

            public T read(io.netty.buffer.ByteBuf in) {
                return codec.decode((RegistryFriendlyByteBuf) in);
            }
        };
    }
    public interface ForValueType<T> extends EntityDataSerializer<T> {
        default T copy(T value) {
            return value;
        }
    }
}
