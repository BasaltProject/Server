// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.InteractionHand;
public record ServerboundUseItemPacket(InteractionHand hand, int sequence, float yRot, float xRot) implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ServerboundUseItemPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundUseItemPacket.STREAM_CODEC", () -> StreamCodec.composite(InteractionHand.STREAM_CODEC, ServerboundUseItemPacket::hand, ByteBufCodecs.VAR_INT, ServerboundUseItemPacket::sequence, ByteBufCodecs.FLOAT, ServerboundUseItemPacket::yRot, ByteBufCodecs.FLOAT, ServerboundUseItemPacket::xRot, ServerboundUseItemPacket::new));
    public PacketType<ServerboundUseItemPacket> type() {
        return GamePacketTypes.SERVERBOUND_USE_ITEM;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleUseItem(this);
    }
}
