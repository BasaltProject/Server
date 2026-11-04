// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.Difficulty;
public record ClientboundChangeDifficultyPacket(Difficulty difficulty, boolean locked) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<ByteBuf, ClientboundChangeDifficultyPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundChangeDifficultyPacket.STREAM_CODEC", () -> StreamCodec.composite(Difficulty.STREAM_CODEC, ClientboundChangeDifficultyPacket::difficulty, ByteBufCodecs.BOOL, ClientboundChangeDifficultyPacket::locked, ClientboundChangeDifficultyPacket::new));
    public PacketType<ClientboundChangeDifficultyPacket> type() {
        return GamePacketTypes.CLIENTBOUND_CHANGE_DIFFICULTY;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleChangeDifficulty(this);
    }
}
