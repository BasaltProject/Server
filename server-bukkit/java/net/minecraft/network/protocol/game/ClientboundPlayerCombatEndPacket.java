// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.damagesource.CombatTracker;
public class ClientboundPlayerCombatEndPacket implements Packet<ClientGamePacketListener> {
    private final int duration;
    public static final StreamCodec<FriendlyByteBuf, ClientboundPlayerCombatEndPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundPlayerCombatEndPacket", () -> Packet.codec(ClientboundPlayerCombatEndPacket::write, ClientboundPlayerCombatEndPacket::new));
    public ClientboundPlayerCombatEndPacket(CombatTracker tracker) {
        this(tracker.getCombatDuration());
    }
    public ClientboundPlayerCombatEndPacket(int duration) {
        this.duration = duration;
    }
    private ClientboundPlayerCombatEndPacket(FriendlyByteBuf input) {
        this.duration = input.readVarInt();
    }
    private void write(FriendlyByteBuf output) {
        output.writeVarInt(this.duration);
    }
    public PacketType<ClientboundPlayerCombatEndPacket> type() {
        return GamePacketTypes.CLIENTBOUND_PLAYER_COMBAT_END;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handlePlayerCombatEnd(this);
    }
}
