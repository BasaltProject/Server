// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.handshake;

import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;

public record ClientIntentionPacket(int protocolVersion, String hostName, int port, ClientIntent intention) implements Packet<ServerHandshakePacketListener> {
    private static final int kMaxHostLength = 255;
    public static final StreamCodec<FriendlyByteBuf, ClientIntentionPacket> STREAM_CODEC =
        Packet.codec(ClientIntentionPacket::write, ClientIntentionPacket::new);

    private ClientIntentionPacket(FriendlyByteBuf input) {
        this(input.readVarInt(), input.readUtf(kMaxHostLength), input.readUnsignedShort(), ClientIntent.byId(input.readVarInt()));
    }

    private void write(FriendlyByteBuf output) {
        output.writeVarInt(protocolVersion);
        output.writeUtf(hostName);
        output.writeShort(port);
        output.writeVarInt(intention.id());
    }

    public PacketType<ClientIntentionPacket> type() {
        return HandshakePacketTypes.CLIENT_INTENTION;
    }

    public void handle(ServerHandshakePacketListener listener) {
        listener.handleIntention(this);
    }

    public boolean isTerminal() {
        return true;
    }
}
