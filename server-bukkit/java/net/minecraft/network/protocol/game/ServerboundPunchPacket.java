// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ServerboundPunchPacket() implements Packet<ServerGamePacketListener> {
    public static final ServerboundPunchPacket INSTANCE = new ServerboundPunchPacket();
    public static final StreamCodec<ByteBuf, ServerboundPunchPacket> STREAM_CODEC = StreamCodec.unit(INSTANCE);
    public PacketType<ServerboundPunchPacket> type() {
        return GamePacketTypes.SERVERBOUND_PUNCH;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handlePunch(this);
    }
}
