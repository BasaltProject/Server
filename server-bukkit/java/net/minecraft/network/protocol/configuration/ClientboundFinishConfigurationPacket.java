// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.configuration;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ClientboundFinishConfigurationPacket implements Packet<ClientConfigurationPacketListener> {
    public static final ClientboundFinishConfigurationPacket INSTANCE = new ClientboundFinishConfigurationPacket();
    public static final StreamCodec<ByteBuf, ClientboundFinishConfigurationPacket> STREAM_CODEC = StreamCodec.unit(INSTANCE);
    private ClientboundFinishConfigurationPacket() {

    }
    public PacketType<ClientboundFinishConfigurationPacket> type() {
        return ConfigurationPacketTypes.CLIENTBOUND_FINISH_CONFIGURATION;
    }
    public void handle(ClientConfigurationPacketListener listener) {
        listener.handleConfigurationFinished(this);
    }
    public boolean isTerminal() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.configuration.ClientboundFinishConfigurationPacket#isTerminal");
    }
}
