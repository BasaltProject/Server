// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ServerboundLockDifficultyPacket implements Packet<ServerGamePacketListener> {
    private final boolean locked;
    public static final StreamCodec<FriendlyByteBuf, ServerboundLockDifficultyPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundLockDifficultyPacket", () -> Packet.codec(ServerboundLockDifficultyPacket::write, ServerboundLockDifficultyPacket::new));
    public ServerboundLockDifficultyPacket(boolean locked) {
        this.locked = locked;
    }
    private ServerboundLockDifficultyPacket(FriendlyByteBuf input) {
        this.locked = input.readBoolean();
    }
    private void write(FriendlyByteBuf output) {
        output.writeBoolean(this.locked);
    }
    public PacketType<ServerboundLockDifficultyPacket> type() {
        return GamePacketTypes.SERVERBOUND_LOCK_DIFFICULTY;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleLockDifficulty(this);
    }
    public boolean isLocked() {
        return locked;
    }
}
