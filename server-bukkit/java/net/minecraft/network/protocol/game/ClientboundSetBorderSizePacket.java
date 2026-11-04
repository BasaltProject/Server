// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.border.WorldBorder;
public class ClientboundSetBorderSizePacket implements Packet<ClientGamePacketListener> {
    private final double size;
    public static final StreamCodec<FriendlyByteBuf, ClientboundSetBorderSizePacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundSetBorderSizePacket", () -> Packet.codec(ClientboundSetBorderSizePacket::write, ClientboundSetBorderSizePacket::new));
    public ClientboundSetBorderSizePacket(WorldBorder border) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundSetBorderSizePacket#<init>");
    }
    private ClientboundSetBorderSizePacket(FriendlyByteBuf input) {
        this.size = input.readDouble();
    }
    private void write(FriendlyByteBuf output) {
        output.writeDouble(this.size);
    }
    public PacketType<ClientboundSetBorderSizePacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_BORDER_SIZE;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSetBorderSize(this);
    }
    public double getSize() {
        return size;
    }
}
