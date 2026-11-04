// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.login;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ClientboundLoginCompressionPacket implements Packet<ClientLoginPacketListener> {
    private final int compressionThreshold;
    public static final StreamCodec<FriendlyByteBuf, ClientboundLoginCompressionPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundLoginCompressionPacket", () -> Packet.codec(ClientboundLoginCompressionPacket::write, ClientboundLoginCompressionPacket::new));
    public ClientboundLoginCompressionPacket(int compressionThreshold) {
        this.compressionThreshold = compressionThreshold;
    }
    private ClientboundLoginCompressionPacket(FriendlyByteBuf input) {
        this.compressionThreshold = input.readVarInt();
    }
    private void write(FriendlyByteBuf output) {
        output.writeVarInt(this.compressionThreshold);
    }
    public PacketType<ClientboundLoginCompressionPacket> type() {
        return LoginPacketTypes.CLIENTBOUND_LOGIN_COMPRESSION;
    }
    public void handle(ClientLoginPacketListener listener) {
        listener.handleCompression(this);
    }
    public int getCompressionThreshold() {
        return compressionThreshold;
    }
}
