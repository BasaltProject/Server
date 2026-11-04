// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.InteractionHand;
public record ClientboundOpenBookPacket(InteractionHand hand) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundOpenBookPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundOpenBookPacket.STREAM_CODEC", () -> StreamCodec.composite(InteractionHand.STREAM_CODEC, ClientboundOpenBookPacket::hand, ClientboundOpenBookPacket::new));
    public PacketType<ClientboundOpenBookPacket> type() {
        return GamePacketTypes.CLIENTBOUND_OPEN_BOOK;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleOpenBook(this);
    }
}
