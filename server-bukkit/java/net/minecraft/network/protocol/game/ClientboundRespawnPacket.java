// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundRespawnPacket(CommonPlayerSpawnInfo commonPlayerSpawnInfo, byte dataToKeep) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundRespawnPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundRespawnPacket.STREAM_CODEC", () -> StreamCodec.composite(CommonPlayerSpawnInfo.STREAM_CODEC, ClientboundRespawnPacket::commonPlayerSpawnInfo, ByteBufCodecs.BYTE, ClientboundRespawnPacket::dataToKeep, ClientboundRespawnPacket::new));
    public static final byte KEEP_ATTRIBUTE_MODIFIERS = (byte) 1;
    public static final byte KEEP_ENTITY_DATA = (byte) 2;
    public static final byte KEEP_ALL_DATA = (byte) 3;
    public PacketType<ClientboundRespawnPacket> type() {
        return GamePacketTypes.CLIENTBOUND_RESPAWN;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleRespawn(this);
    }
    public boolean shouldKeep(byte mask) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundRespawnPacket#shouldKeep");
    }
}
