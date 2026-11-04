// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.common.custom;

import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;

public record DiscardedPayload(Identifier id, byte[] data) implements CustomPacketPayload {
    public DiscardedPayload(Identifier id) {
        this(id, new byte[0]);
    }

    public static <T extends FriendlyByteBuf> StreamCodec<T, DiscardedPayload> codec(Identifier id, int maxPayloadSize) {
        return CustomPacketPayload.codec((payload, buf) -> buf.writeBytes(payload.data()), buf -> {
            int length = buf.readableBytes();
            if (length < 0 || length > maxPayloadSize) {
                throw new IllegalArgumentException("Payload may not be larger than " + maxPayloadSize + " bytes");
            }
            byte[] data = new byte[length];
            buf.readBytes(data);
            return new DiscardedPayload(id, data);
        });
    }

    public CustomPacketPayload.Type<DiscardedPayload> type() {
        return new CustomPacketPayload.Type<>(id);
    }
}
