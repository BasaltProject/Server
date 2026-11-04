// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ServerboundConfigurationAcknowledgedPacket implements Packet<ServerGamePacketListener> {
    public static final ServerboundConfigurationAcknowledgedPacket INSTANCE = new ServerboundConfigurationAcknowledgedPacket();
    public static final StreamCodec<ByteBuf, ServerboundConfigurationAcknowledgedPacket> STREAM_CODEC = StreamCodec.unit(INSTANCE);
    private ServerboundConfigurationAcknowledgedPacket() {

    }
    public PacketType<ServerboundConfigurationAcknowledgedPacket> type() {
        return GamePacketTypes.SERVERBOUND_CONFIGURATION_ACKNOWLEDGED;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleConfigurationAcknowledged(this);
    }
    public boolean isTerminal() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerboundConfigurationAcknowledgedPacket#isTerminal");
    }
}
