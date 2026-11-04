// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.common;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ClientboundKeepAlivePacket implements Packet<ClientCommonPacketListener> {
    private final long id;
    public static final StreamCodec<FriendlyByteBuf, ClientboundKeepAlivePacket> STREAM_CODEC = Packet.codec(ClientboundKeepAlivePacket::write, ClientboundKeepAlivePacket::new);
    public ClientboundKeepAlivePacket(long id) {
        this.id = id;
    }
    private ClientboundKeepAlivePacket(FriendlyByteBuf input) {
        this.id = input.readLong();
    }
    private void write(FriendlyByteBuf output) {
        output.writeLong(this.id);
    }
    public PacketType<ClientboundKeepAlivePacket> type() {
        return CommonPacketTypes.CLIENTBOUND_KEEP_ALIVE;
    }
    public void handle(ClientCommonPacketListener listener) {
        listener.handleKeepAlive(this);
    }
    public long getId() {
        return id;
    }
}
