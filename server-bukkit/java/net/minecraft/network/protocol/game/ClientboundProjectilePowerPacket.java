// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ClientboundProjectilePowerPacket implements Packet<ClientGamePacketListener> {
    private final int id;
    private final double accelerationPower;
    public static final StreamCodec<FriendlyByteBuf, ClientboundProjectilePowerPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundProjectilePowerPacket", () -> Packet.codec(ClientboundProjectilePowerPacket::write, ClientboundProjectilePowerPacket::new));
    public ClientboundProjectilePowerPacket(int id, double accelerationPower) {
        this.id = id;
        this.accelerationPower = accelerationPower;
    }
    private ClientboundProjectilePowerPacket(FriendlyByteBuf input) {
        this.id = input.readVarInt();
        this.accelerationPower = input.readDouble();
    }
    private void write(FriendlyByteBuf output) {
        output.writeVarInt(this.id);
        output.writeDouble(this.accelerationPower);
    }
    public PacketType<ClientboundProjectilePowerPacket> type() {
        return GamePacketTypes.CLIENTBOUND_PROJECTILE_POWER;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleProjectilePowerPacket(this);
    }
    public int getId() {
        return id;
    }
    public double getAccelerationPower() {
        return accelerationPower;
    }
}
