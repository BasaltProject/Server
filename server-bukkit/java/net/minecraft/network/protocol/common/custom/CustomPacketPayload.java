// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.common.custom;

import io.netty.buffer.ByteBuf;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.codec.StreamDecoder;
import net.minecraft.network.codec.StreamMemberEncoder;
import net.minecraft.resources.Identifier;

public interface CustomPacketPayload {
    Type<? extends CustomPacketPayload> type();

    static <B extends ByteBuf, T extends CustomPacketPayload> StreamCodec<B, T> codec(StreamMemberEncoder<B, T> writer, StreamDecoder<B, T> reader) {
        return StreamCodec.ofMember(writer, reader);
    }

    static <T extends CustomPacketPayload> Type<T> createType(String id) {
        return new Type<>(Identifier.withDefaultNamespace(id));
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    static <B extends FriendlyByteBuf> StreamCodec<B, CustomPacketPayload> codec(FallbackProvider<B> fallback, List<TypeAndCodec<? super B, ?>> types) {
        Map<Identifier, StreamCodec> byId = new HashMap<>();
        for (TypeAndCodec<? super B, ?> entry : types) {
            byId.put(entry.type().id(), entry.codec());
        }
        return new StreamCodec<B, CustomPacketPayload>() {
            private StreamCodec codecFor(Identifier id) {
                StreamCodec found = byId.get(id);
                return found != null ? found : fallback.create(id);
            }

            @Override
            public void encode(B output, CustomPacketPayload value) {
                Identifier id = value.type().id();
                output.writeIdentifier(id);
                StreamCodec own = byId.containsKey(id) ? null : declaredCodec(value.getClass());
                (own != null ? own : codecFor(id)).encode(output, value);
            }

            @Override
            public CustomPacketPayload decode(B input) {
                Identifier id = input.readIdentifier();
                return (CustomPacketPayload) codecFor(id).decode(input);
            }
        };
    }

    @SuppressWarnings("rawtypes")
    private static StreamCodec declaredCodec(Class<?> type) {
        try {
            java.lang.reflect.Field field = type.getDeclaredField("STREAM_CODEC");
            field.setAccessible(true);
            return java.lang.reflect.Modifier.isStatic(field.getModifiers()) ? (StreamCodec) field.get(null) : null;
        } catch (ReflectiveOperationException | RuntimeException failure) {
            return null;
        }
    }

    public static record Type<T extends CustomPacketPayload>(Identifier id) {}

    public static record TypeAndCodec<B extends FriendlyByteBuf, T extends CustomPacketPayload>(Type<T> type, StreamCodec<B, T> codec) {}

    public interface FallbackProvider<B extends FriendlyByteBuf> {
        StreamCodec<B, ? extends CustomPacketPayload> create(Identifier typeId);
    }
}
