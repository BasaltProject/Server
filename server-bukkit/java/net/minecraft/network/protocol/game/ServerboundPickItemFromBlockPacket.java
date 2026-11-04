// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import net.minecraft.core.BlockPos;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ServerboundPickItemFromBlockPacket(BlockPos pos, boolean includeData) implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<ByteBuf, ServerboundPickItemFromBlockPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundPickItemFromBlockPacket.STREAM_CODEC", () -> StreamCodec.composite(BlockPos.STREAM_CODEC, ServerboundPickItemFromBlockPacket::pos, ByteBufCodecs.BOOL, ServerboundPickItemFromBlockPacket::includeData, ServerboundPickItemFromBlockPacket::new));
    public PacketType<ServerboundPickItemFromBlockPacket> type() {
        return GamePacketTypes.SERVERBOUND_PICK_ITEM_FROM_BLOCK;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handlePickItemFromBlock(this);
    }
}
