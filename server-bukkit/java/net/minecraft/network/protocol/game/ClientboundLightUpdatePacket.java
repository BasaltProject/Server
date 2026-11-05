// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import org.jspecify.annotations.Nullable;
import io.netty.buffer.ByteBuf;
import java.util.BitSet;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.lighting.LevelLightEngine;
public record ClientboundLightUpdatePacket(int x, int z, ClientboundLightUpdatePacketData lightData) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<ByteBuf, ClientboundLightUpdatePacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundLightUpdatePacket.STREAM_CODEC", () -> StreamCodec.composite(ByteBufCodecs.VAR_INT, ClientboundLightUpdatePacket::x, ByteBufCodecs.VAR_INT, ClientboundLightUpdatePacket::z, ClientboundLightUpdatePacketData.STREAM_CODEC, ClientboundLightUpdatePacket::lightData, ClientboundLightUpdatePacket::new));
    public ClientboundLightUpdatePacket(ChunkPos pos, LevelLightEngine lightEngine, BitSet skyChangedLightSectionFilter, BitSet blockChangedLightSectionFilter) {
        this(pos.x(), pos.z(), new ClientboundLightUpdatePacketData(pos, lightEngine, skyChangedLightSectionFilter, blockChangedLightSectionFilter));
    }
    public PacketType<ClientboundLightUpdatePacket> type() {
        return GamePacketTypes.CLIENTBOUND_LIGHT_UPDATE;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleLightUpdatePacket(this);
    }
}
