// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.common;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ClientboundPingPacket implements Packet<ClientCommonPacketListener> {
    private final int id;
    public static final StreamCodec<FriendlyByteBuf, ClientboundPingPacket> STREAM_CODEC = Packet.codec(ClientboundPingPacket::write, ClientboundPingPacket::new);
    public ClientboundPingPacket(int id) {
        this.id = id;
    }
    private ClientboundPingPacket(FriendlyByteBuf input) {
        this.id = input.readInt();
    }
    private void write(FriendlyByteBuf output) {
        output.writeInt(this.id);
    }
    public PacketType<ClientboundPingPacket> type() {
        return CommonPacketTypes.CLIENTBOUND_PING;
    }
    public void handle(ClientCommonPacketListener listener) {
        listener.handlePing(this);
    }
    public int getId() {
        return id;
    }
}
