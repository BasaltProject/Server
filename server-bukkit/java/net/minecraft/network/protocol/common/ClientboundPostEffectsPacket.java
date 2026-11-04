// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.common;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import java.util.List;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.resources.Identifier;
public record ClientboundPostEffectsPacket(List<Identifier> postEffects) implements Packet<ClientCommonPacketListener> {
    public static final StreamCodec<ByteBuf, ClientboundPostEffectsPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundPostEffectsPacket.STREAM_CODEC", () -> StreamCodec.composite(Identifier.STREAM_CODEC.apply(ByteBufCodecs.list()), ClientboundPostEffectsPacket::postEffects, ClientboundPostEffectsPacket::new));
    public PacketType<ClientboundPostEffectsPacket> type() {
        return CommonPacketTypes.CLIENTBOUND_POST_EFFECTS;
    }
    public void handle(ClientCommonPacketListener listener) {
        listener.handlePostEffects(this);
    }
}
