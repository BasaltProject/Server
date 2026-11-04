// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ServerboundAttackPacket(int entityId) implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<ByteBuf, ServerboundAttackPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundAttackPacket.STREAM_CODEC", () -> StreamCodec.composite(ByteBufCodecs.VAR_INT, ServerboundAttackPacket::entityId, ServerboundAttackPacket::new));
    public PacketType<ServerboundAttackPacket> type() {
        return GamePacketTypes.SERVERBOUND_ATTACK;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleAttack(this);
    }
}
