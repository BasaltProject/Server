// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ClientboundSetHealthPacket implements Packet<ClientGamePacketListener> {
    private final float health;
    private final int food;
    private final float saturation;
    public static final StreamCodec<FriendlyByteBuf, ClientboundSetHealthPacket> STREAM_CODEC = Packet.codec(ClientboundSetHealthPacket::write, ClientboundSetHealthPacket::new);
    public ClientboundSetHealthPacket(float health, int food, float saturation) {
        this.health = health;
        this.food = food;
        this.saturation = saturation;
    }
    private ClientboundSetHealthPacket(FriendlyByteBuf input) {
        this.health = input.readFloat();
        this.food = input.readVarInt();
        this.saturation = input.readFloat();
    }
    private void write(FriendlyByteBuf output) {
        output.writeFloat(this.health);
        output.writeVarInt(this.food);
        output.writeFloat(this.saturation);
    }
    public PacketType<ClientboundSetHealthPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_HEALTH;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSetHealth(this);
    }
    public float getHealth() {
        return health;
    }
    public int getFood() {
        return food;
    }
    public float getSaturation() {
        return saturation;
    }
}
