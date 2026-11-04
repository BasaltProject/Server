// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.login;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.network.protocol.login.custom.CustomQueryPayload;
public record ClientboundCustomQueryPacket(int transactionId, CustomQueryPayload payload) implements Packet<ClientLoginPacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundCustomQueryPacket> STREAM_CODEC = Packet.codec(ClientboundCustomQueryPacket::write, ClientboundCustomQueryPacket::new);
    private ClientboundCustomQueryPacket(FriendlyByteBuf input) {
        this(input.readVarInt(), readPayload(input));
    }

    private static CustomQueryPayload readPayload(FriendlyByteBuf input) {
        net.minecraft.resources.Identifier id = input.readIdentifier();
        input.skipBytes(input.readableBytes());
        return new net.minecraft.network.protocol.login.custom.DiscardedQueryPayload(id);
    }

    private void write(FriendlyByteBuf output) {
        output.writeVarInt(transactionId);
        output.writeIdentifier(payload.id());
        payload.write(output);
    }
    public PacketType<ClientboundCustomQueryPacket> type() {
        return LoginPacketTypes.CLIENTBOUND_CUSTOM_QUERY;
    }
    public void handle(ClientLoginPacketListener listener) {
        listener.handleCustomQuery(this);
    }
}
