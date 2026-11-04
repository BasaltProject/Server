// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.login;
import net.minecraft.network.protocol.PacketFlow;
import net.minecraft.network.protocol.PacketType;
public class LoginPacketTypes {
    public static final PacketType<ClientboundCustomQueryPacket> CLIENTBOUND_CUSTOM_QUERY = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("custom_query"));
    public static final PacketType<ClientboundLoginFinishedPacket> CLIENTBOUND_LOGIN_FINISHED = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("login_finished"));
    public static final PacketType<ClientboundHelloPacket> CLIENTBOUND_HELLO = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("hello"));
    public static final PacketType<ClientboundLoginCompressionPacket> CLIENTBOUND_LOGIN_COMPRESSION = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("login_compression"));
    public static final PacketType<ClientboundLoginDisconnectPacket> CLIENTBOUND_LOGIN_DISCONNECT = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("login_disconnect"));
    public static final PacketType<ServerboundCustomQueryAnswerPacket> SERVERBOUND_CUSTOM_QUERY_ANSWER = new PacketType<>(PacketFlow.SERVERBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("custom_query_answer"));
    public static final PacketType<ServerboundHelloPacket> SERVERBOUND_HELLO = new PacketType<>(PacketFlow.SERVERBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("hello"));
    public static final PacketType<ServerboundKeyPacket> SERVERBOUND_KEY = new PacketType<>(PacketFlow.SERVERBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("key"));
    public static final PacketType<ServerboundLoginAcknowledgedPacket> SERVERBOUND_LOGIN_ACKNOWLEDGED = new PacketType<>(PacketFlow.SERVERBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("login_acknowledged"));
}
