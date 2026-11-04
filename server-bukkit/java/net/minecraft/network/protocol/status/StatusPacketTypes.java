// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.status;
import net.minecraft.network.protocol.PacketFlow;
import net.minecraft.network.protocol.PacketType;
public class StatusPacketTypes {
    public static final PacketType<ClientboundStatusResponsePacket> CLIENTBOUND_STATUS_RESPONSE = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("status_response"));
    public static final PacketType<ServerboundStatusRequestPacket> SERVERBOUND_STATUS_REQUEST = new PacketType<>(PacketFlow.SERVERBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("status_request"));
}
