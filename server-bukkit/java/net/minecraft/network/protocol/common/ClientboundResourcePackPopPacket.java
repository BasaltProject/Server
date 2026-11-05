// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.common;
import net.minecraft.core.UUIDUtil;
import java.util.Optional;
import java.util.UUID;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundResourcePackPopPacket(Optional<UUID> id) implements Packet<ClientCommonPacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundResourcePackPopPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundResourcePackPopPacket", () -> StreamCodec.of((output, packet) -> {
            output.writeOptional(packet.id(), UUIDUtil.STREAM_CODEC);
        }, input -> new ClientboundResourcePackPopPacket(input.readOptional(UUIDUtil.STREAM_CODEC))));
    private ClientboundResourcePackPopPacket(FriendlyByteBuf input) {
        this(input.readOptional(UUIDUtil.STREAM_CODEC));
    }
    public PacketType<ClientboundResourcePackPopPacket> type() {
        return CommonPacketTypes.CLIENTBOUND_RESOURCE_PACK_POP;
    }
    public void handle(ClientCommonPacketListener listener) {
        listener.handleResourcePackPop(this);
    }
}
