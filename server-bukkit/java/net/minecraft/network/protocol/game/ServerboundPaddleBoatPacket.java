// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ServerboundPaddleBoatPacket implements Packet<ServerGamePacketListener> {
    private final boolean left;
    private final boolean right;
    public static final StreamCodec<FriendlyByteBuf, ServerboundPaddleBoatPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundPaddleBoatPacket", () -> Packet.codec(ServerboundPaddleBoatPacket::write, ServerboundPaddleBoatPacket::new));
    public ServerboundPaddleBoatPacket(boolean left, boolean right) {
        this.left = left;
        this.right = right;
    }
    private ServerboundPaddleBoatPacket(FriendlyByteBuf input) {
        this.left = input.readBoolean();
        this.right = input.readBoolean();
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handlePaddleBoat(this);
    }
    private void write(FriendlyByteBuf output) {
        output.writeBoolean(this.left);
        output.writeBoolean(this.right);
    }
    public PacketType<ServerboundPaddleBoatPacket> type() {
        return GamePacketTypes.SERVERBOUND_PADDLE_BOAT;
    }
    public boolean getLeft() {
        return left;
    }
    public boolean getRight() {
        return right;
    }
}
