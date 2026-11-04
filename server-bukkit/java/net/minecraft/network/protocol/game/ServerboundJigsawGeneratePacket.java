// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.core.BlockPos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ServerboundJigsawGeneratePacket implements Packet<ServerGamePacketListener> {
    private final BlockPos pos;
    private final int levels;
    private final boolean keepJigsaws;
    public static final StreamCodec<FriendlyByteBuf, ServerboundJigsawGeneratePacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundJigsawGeneratePacket", () -> Packet.codec(ServerboundJigsawGeneratePacket::write, ServerboundJigsawGeneratePacket::new));
    public ServerboundJigsawGeneratePacket(BlockPos blockPos, int levels, boolean keepJigsaws) {
        this.pos = blockPos;
        this.levels = levels;
        this.keepJigsaws = keepJigsaws;
    }
    private ServerboundJigsawGeneratePacket(FriendlyByteBuf input) {
        this.pos = input.readBlockPos();
        this.levels = input.readVarInt();
        this.keepJigsaws = input.readBoolean();
    }
    private void write(FriendlyByteBuf output) {
        output.writeBlockPos(this.pos);
        output.writeVarInt(this.levels);
        output.writeBoolean(this.keepJigsaws);
    }
    public PacketType<ServerboundJigsawGeneratePacket> type() {
        return GamePacketTypes.SERVERBOUND_JIGSAW_GENERATE;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleJigsawGenerate(this);
    }
    public BlockPos getPos() {
        return pos;
    }
    public int levels() {
        return levels;
    }
    public boolean keepJigsaws() {
        return keepJigsaws;
    }
}
