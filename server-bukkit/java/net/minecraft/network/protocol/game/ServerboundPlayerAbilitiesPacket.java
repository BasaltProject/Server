// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.entity.player.Abilities;
public class ServerboundPlayerAbilitiesPacket implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ServerboundPlayerAbilitiesPacket> STREAM_CODEC = Packet.codec(ServerboundPlayerAbilitiesPacket::write, ServerboundPlayerAbilitiesPacket::new);
    private final boolean isFlying;
    public ServerboundPlayerAbilitiesPacket(Abilities abilities) {
        this.isFlying = abilities.flying;
    }
    private ServerboundPlayerAbilitiesPacket(FriendlyByteBuf input) {
        this.isFlying = (input.readByte() & 2) != 0;
    }
    private void write(FriendlyByteBuf output) {
        output.writeByte(isFlying ? 2 : 0);
    }
    public PacketType<ServerboundPlayerAbilitiesPacket> type() {
        return GamePacketTypes.SERVERBOUND_PLAYER_ABILITIES;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handlePlayerAbilities(this);
    }
    public boolean isFlying() {
        return isFlying;
    }
}
