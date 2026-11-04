// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.ping;
import net.minecraft.network.protocol.PacketFlow;
import net.minecraft.network.protocol.PacketType;
public class PingPacketTypes {
    public static final PacketType<ClientboundPongResponsePacket> CLIENTBOUND_PONG_RESPONSE = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("pong_response"));
    public static final PacketType<ServerboundPingRequestPacket> SERVERBOUND_PING_REQUEST = new PacketType<>(PacketFlow.SERVERBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("ping_request"));
}
