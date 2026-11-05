// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import java.util.Objects;
import net.minecraft.network.codec.ByteBufCodecs;
import org.jspecify.annotations.Nullable;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.saveddata.maps.MapDecoration;
import net.minecraft.world.level.saveddata.maps.MapId;
import net.minecraft.world.level.saveddata.maps.MapItemSavedData;
public record ClientboundMapItemDataPacket(MapId mapId, byte scale, boolean locked, Optional<List<MapDecoration>> decorations, Optional<MapItemSavedData.MapPatch> colorPatch) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundMapItemDataPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundMapItemDataPacket.STREAM_CODEC", () -> StreamCodec.composite(MapId.STREAM_CODEC, ClientboundMapItemDataPacket::mapId, ByteBufCodecs.BYTE, ClientboundMapItemDataPacket::scale, ByteBufCodecs.BOOL, ClientboundMapItemDataPacket::locked, MapDecoration.STREAM_CODEC.apply(ByteBufCodecs.list()).apply(ByteBufCodecs::optional), ClientboundMapItemDataPacket::decorations, MapItemSavedData.MapPatch.STREAM_CODEC, ClientboundMapItemDataPacket::colorPatch, ClientboundMapItemDataPacket::new));
    public ClientboundMapItemDataPacket(MapId mapId, byte scale, boolean locked, Collection<MapDecoration> decorations, MapItemSavedData. MapPatch colorPatch) {
        this(mapId, scale, locked, decorations != null ? Optional.of(List.copyOf(decorations)) : Optional.empty(), Optional.ofNullable(colorPatch));
    }
    public PacketType<ClientboundMapItemDataPacket> type() {
        return GamePacketTypes.CLIENTBOUND_MAP_ITEM_DATA;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleMapItemData(this);
    }
    public void applyToMap(MapItemSavedData map) {
        decorations.ifPresent(map::addClientSideDecorations);
        colorPatch.ifPresent(patch -> patch.applyToMap(map));
    }
}
