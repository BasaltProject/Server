// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import java.util.List;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.entity.Entity;
public class ClientboundSetPassengersPacket implements Packet<ClientGamePacketListener> {
    private final int vehicle;
    private final int[] passengers;
    public static final StreamCodec<FriendlyByteBuf, ClientboundSetPassengersPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundSetPassengersPacket", () -> Packet.codec(ClientboundSetPassengersPacket::write, ClientboundSetPassengersPacket::new));
    public ClientboundSetPassengersPacket(Entity vehicle) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundSetPassengersPacket#<init>");
    }
    private ClientboundSetPassengersPacket(FriendlyByteBuf input) {
        this.vehicle = input.readVarInt();
        this.passengers = input.readVarIntArray();
    }
    private void write(FriendlyByteBuf output) {
        output.writeVarInt(this.vehicle);
        output.writeVarIntArray(this.passengers);
    }
    public PacketType<ClientboundSetPassengersPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_PASSENGERS;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSetEntityPassengersPacket(this);
    }
    public int[] getPassengers() {
        return passengers;
    }
    public int getVehicle() {
        return vehicle;
    }
}
