package net.minecraft.network.syncher;
import io.netty.buffer.ByteBuf;
import java.util.function.BiConsumer;
import java.util.function.Function;
import org.basaltmc.bukkit.net.PacketCodec;
public final class EntityDataSerializers {
    private EntityDataSerializers() {}
    private static <T> EntityDataSerializer<T> serializer(
        int id, BiConsumer<ByteBuf, T> writer, Function<ByteBuf, T> reader) {
        return new EntityDataSerializer<>() {
            public int id() {
                return id;
            }
            public void write(ByteBuf out, T value) {
                writer.accept(out, value);
            }
            public T read(ByteBuf in) {
                return reader.apply(in);
            }
        };
    }
    public static final EntityDataSerializer<Byte> BYTE =
        serializer(0, (out, v) -> out.writeByte(v), ByteBuf::readByte);
    public static final EntityDataSerializer<Integer> INT = serializer(1, PacketCodec::varInt, PacketCodec::readVarInt);
    public static final EntityDataSerializer<Long> LONG = serializer(2, PacketCodec::varLong, PacketCodec::readVarLong);
    public static final EntityDataSerializer<Float> FLOAT =
        serializer(3, (out, v) -> out.writeFloat(v), ByteBuf::readFloat);
    public static final EntityDataSerializer<String> STRING =
        serializer(4, PacketCodec::string, PacketCodec::readString);
    public static final EntityDataSerializer<Boolean> BOOLEAN =
        serializer(8, (out, v) -> out.writeBoolean(v), ByteBuf::readBoolean);
    public static EntityDataSerializer<?> getSerializer(int id) {
        return switch (id) {
            case 0 -> BYTE;
            case 1 -> INT;
            case 2 -> LONG;
            case 3 -> FLOAT;
            case 4 -> STRING;
            case 8 -> BOOLEAN;
            default -> null;
        };
    }
    public static int getSerializedId(EntityDataSerializer<?> serializer) {
        return serializer.id();
    }
}
