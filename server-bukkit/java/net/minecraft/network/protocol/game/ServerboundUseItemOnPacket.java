// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.phys.BlockHitResult;
public record ServerboundUseItemOnPacket(InteractionHand hand, BlockHitResult hitResult, int sequence) implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ServerboundUseItemOnPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundUseItemOnPacket.STREAM_CODEC", () -> StreamCodec.composite(InteractionHand.STREAM_CODEC, ServerboundUseItemOnPacket::hand, BlockHitResult.STREAM_CODEC, ServerboundUseItemOnPacket::hitResult, ByteBufCodecs.VAR_INT, ServerboundUseItemOnPacket::sequence, ServerboundUseItemOnPacket::new));
    public PacketType<ServerboundUseItemOnPacket> type() {
        return GamePacketTypes.SERVERBOUND_USE_ITEM_ON;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleUseItemOn(this);
    }
}
