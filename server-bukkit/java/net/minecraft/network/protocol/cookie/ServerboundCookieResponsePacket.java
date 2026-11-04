// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.cookie;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.resources.Identifier;
public record ServerboundCookieResponsePacket(Identifier key, byte [] payload) implements Packet<ServerCookiePacketListener> {
    private static final int kMaxCookieLength = 5120;
    public static final StreamCodec<FriendlyByteBuf, ServerboundCookieResponsePacket> STREAM_CODEC = Packet.codec(ServerboundCookieResponsePacket::write, ServerboundCookieResponsePacket::new);
    private ServerboundCookieResponsePacket(FriendlyByteBuf input) {
        this(input.readIdentifier(), input.readNullable((FriendlyByteBuf buf) -> buf.readByteArray(kMaxCookieLength)));
    }
    private void write(FriendlyByteBuf output) {
        output.writeIdentifier(key);
        output.writeNullable(payload, (FriendlyByteBuf out, byte[] bytes) -> out.writeByteArray(bytes));
    }
    public PacketType<ServerboundCookieResponsePacket> type() {
        return CookiePacketTypes.SERVERBOUND_COOKIE_RESPONSE;
    }
    public void handle(ServerCookiePacketListener listener) {
        listener.handleCookieResponse(this);
    }
}
