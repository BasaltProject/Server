// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.core.BlockPos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ServerboundBlockEntityTagQueryPacket implements Packet<ServerGamePacketListener> {
    private final int transactionId;
    private final BlockPos pos;
    public static final StreamCodec<FriendlyByteBuf, ServerboundBlockEntityTagQueryPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundBlockEntityTagQueryPacket", () -> Packet.codec(ServerboundBlockEntityTagQueryPacket::write, ServerboundBlockEntityTagQueryPacket::new));
    public ServerboundBlockEntityTagQueryPacket(int transactionId, BlockPos pos) {
        this.transactionId = transactionId;
        this.pos = pos;
    }
    private ServerboundBlockEntityTagQueryPacket(FriendlyByteBuf input) {
        this.transactionId = input.readVarInt();
        this.pos = input.readBlockPos();
    }
    private void write(FriendlyByteBuf output) {
        output.writeVarInt(this.transactionId);
        output.writeBlockPos(this.pos);
    }
    public PacketType<ServerboundBlockEntityTagQueryPacket> type() {
        return GamePacketTypes.SERVERBOUND_BLOCK_ENTITY_TAG_QUERY;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleBlockEntityTagQuery(this);
    }
    public int getTransactionId() {
        return transactionId;
    }
    public BlockPos getPos() {
        return pos;
    }
}
