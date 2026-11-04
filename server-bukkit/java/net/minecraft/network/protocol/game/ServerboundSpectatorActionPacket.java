// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import java.util.OptionalInt;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ServerboundSpectatorActionPacket(OptionalInt spectateEntityId) implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<ByteBuf, ServerboundSpectatorActionPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundSpectatorActionPacket.STREAM_CODEC", () -> StreamCodec.composite(ByteBufCodecs.OPTIONAL_VAR_INT, ServerboundSpectatorActionPacket::spectateEntityId, ServerboundSpectatorActionPacket::new));
    public PacketType<ServerboundSpectatorActionPacket> type() {
        return GamePacketTypes.SERVERBOUND_SPECTATOR_ACTION;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleSpectatorAction(this);
    }
}
