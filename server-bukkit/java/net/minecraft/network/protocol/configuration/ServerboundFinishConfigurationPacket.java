// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.configuration;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ServerboundFinishConfigurationPacket implements Packet<ServerConfigurationPacketListener> {
    public static final ServerboundFinishConfigurationPacket INSTANCE = new ServerboundFinishConfigurationPacket();
    public static final StreamCodec<ByteBuf, ServerboundFinishConfigurationPacket> STREAM_CODEC = StreamCodec.unit(INSTANCE);
    private ServerboundFinishConfigurationPacket() {

    }
    public PacketType<ServerboundFinishConfigurationPacket> type() {
        return ConfigurationPacketTypes.SERVERBOUND_FINISH_CONFIGURATION;
    }
    public void handle(ServerConfigurationPacketListener listener) {
        listener.handleConfigurationFinished(this);
    }
    public boolean isTerminal() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.configuration.ServerboundFinishConfigurationPacket#isTerminal");
    }
}
