// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.login;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundLoginDisconnectPacket(Component reason) implements Packet<ClientLoginPacketListener> {
    public static final StreamCodec<ByteBuf, ClientboundLoginDisconnectPacket> STREAM_CODEC = net.minecraft.network.codec.ByteBufCodecs.stringUtf8(262144).map(text -> new ClientboundLoginDisconnectPacket(net.minecraft.network.chat.ComponentSerialization.CODEC.parse(com.mojang.serialization.JsonOps.INSTANCE, com.google.gson.JsonParser.parseString(text)).getOrThrow()), packet -> net.minecraft.network.chat.ComponentSerialization.CODEC.encodeStart(com.mojang.serialization.JsonOps.INSTANCE, packet.reason()).getOrThrow().toString());
    public PacketType<ClientboundLoginDisconnectPacket> type() {
        return LoginPacketTypes.CLIENTBOUND_LOGIN_DISCONNECT;
    }
    public void handle(ClientLoginPacketListener listener) {
        listener.handleDisconnect(this);
    }
}
