// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.core.UUIDUtil;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import java.util.List;
import java.util.UUID;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundPlayerInfoRemovePacket(List<UUID> profileIds) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<ByteBuf, ClientboundPlayerInfoRemovePacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundPlayerInfoRemovePacket.STREAM_CODEC", () -> StreamCodec.composite(UUIDUtil.STREAM_CODEC.apply(ByteBufCodecs.list()), ClientboundPlayerInfoRemovePacket::profileIds, ClientboundPlayerInfoRemovePacket::new));
    public PacketType<ClientboundPlayerInfoRemovePacket> type() {
        return GamePacketTypes.CLIENTBOUND_PLAYER_INFO_REMOVE;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handlePlayerInfoRemove(this);
    }
}
