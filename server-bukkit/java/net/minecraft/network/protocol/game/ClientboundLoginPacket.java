// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import com.google.common.collect.Sets;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.codec.ByteBufCodecs;
import java.util.Set;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.Level;
public record ClientboundLoginPacket(int playerId, boolean hardcore, Set<ResourceKey<Level>> levels, int maxPlayers, int chunkRadius, int simulationDistance, boolean reducedDebugInfo, boolean showDeathScreen, boolean doLimitedCrafting, CommonPlayerSpawnInfo commonPlayerSpawnInfo, boolean onlineMode, boolean enforcesSecureChat) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundLoginPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundLoginPacket.STREAM_CODEC", () -> StreamCodec.composite(ByteBufCodecs.INT, ClientboundLoginPacket::playerId, ByteBufCodecs.BOOL, ClientboundLoginPacket::hardcore, ResourceKey.streamCodec(Registries.DIMENSION).apply(ByteBufCodecs.collection(Sets::newHashSetWithExpectedSize)), ClientboundLoginPacket::levels, ByteBufCodecs.VAR_INT, ClientboundLoginPacket::maxPlayers, ByteBufCodecs.VAR_INT, ClientboundLoginPacket::chunkRadius, ByteBufCodecs.VAR_INT, ClientboundLoginPacket::simulationDistance, ByteBufCodecs.BOOL, ClientboundLoginPacket::reducedDebugInfo, ByteBufCodecs.BOOL, ClientboundLoginPacket::showDeathScreen, ByteBufCodecs.BOOL, ClientboundLoginPacket::doLimitedCrafting, CommonPlayerSpawnInfo.STREAM_CODEC, ClientboundLoginPacket::commonPlayerSpawnInfo, ByteBufCodecs.BOOL, ClientboundLoginPacket::onlineMode, ByteBufCodecs.BOOL, ClientboundLoginPacket::enforcesSecureChat, ClientboundLoginPacket::new));
    public PacketType<ClientboundLoginPacket> type() {
        return GamePacketTypes.CLIENTBOUND_LOGIN;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleLogin(this);
    }
}
