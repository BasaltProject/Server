// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.cookie;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.resources.Identifier;
public record ClientboundCookieRequestPacket(Identifier key) implements Packet<ClientCookiePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundCookieRequestPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundCookieRequestPacket", () -> StreamCodec.of((output, packet) -> {
            output.writeIdentifier(packet.key());
        }, input -> new ClientboundCookieRequestPacket(input.readIdentifier())));
    private ClientboundCookieRequestPacket(FriendlyByteBuf input) {
        this(input.readIdentifier());
    }
    public PacketType<ClientboundCookieRequestPacket> type() {
        return CookiePacketTypes.CLIENTBOUND_COOKIE_REQUEST;
    }
    public void handle(ClientCookiePacketListener listener) {
        listener.handleRequestCookie(this);
    }
}
