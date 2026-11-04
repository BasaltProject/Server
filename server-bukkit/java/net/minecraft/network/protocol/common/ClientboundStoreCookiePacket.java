// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.common;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.resources.Identifier;
public record ClientboundStoreCookiePacket(Identifier key, byte[] payload) implements Packet<ClientCommonPacketListener> {
    private static final int kMaxCookieLength = 5120;
    public static final StreamCodec<FriendlyByteBuf, ClientboundStoreCookiePacket> STREAM_CODEC = Packet.codec(ClientboundStoreCookiePacket::write, ClientboundStoreCookiePacket::new);
    public static final StreamCodec<ByteBuf, byte[]> PAYLOAD_STREAM_CODEC = ByteBufCodecs.byteArray(kMaxCookieLength);
    private ClientboundStoreCookiePacket(FriendlyByteBuf input) {
        this(input.readIdentifier(), PAYLOAD_STREAM_CODEC.decode(input));
    }
    private void write(FriendlyByteBuf output) {
        output.writeIdentifier(key);
        PAYLOAD_STREAM_CODEC.encode(output, payload);
    }
    public PacketType<ClientboundStoreCookiePacket> type() {
        return CommonPacketTypes.CLIENTBOUND_STORE_COOKIE;
    }
    public void handle(ClientCommonPacketListener listener) {
        listener.handleStoreCookie(this);
    }
}
