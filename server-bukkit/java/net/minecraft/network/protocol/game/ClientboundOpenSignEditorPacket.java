// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import io.netty.buffer.ByteBuf;
import net.minecraft.core.BlockPos;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.block.entity.SignTextSlot;
public record ClientboundOpenSignEditorPacket(BlockPos pos, SignTextSlot slot) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<ByteBuf, ClientboundOpenSignEditorPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundOpenSignEditorPacket.STREAM_CODEC", () -> StreamCodec.composite(BlockPos.STREAM_CODEC, ClientboundOpenSignEditorPacket::pos, SignTextSlot.STREAM_CODEC, ClientboundOpenSignEditorPacket::slot, ClientboundOpenSignEditorPacket::new));
    public PacketType<ClientboundOpenSignEditorPacket> type() {
        return GamePacketTypes.CLIENTBOUND_OPEN_SIGN_EDITOR;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleOpenSignEditor(this);
    }
}
