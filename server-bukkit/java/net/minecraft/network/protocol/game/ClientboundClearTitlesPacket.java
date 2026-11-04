// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ClientboundClearTitlesPacket implements Packet<ClientGamePacketListener> {
    private final boolean resetTimes;
    public static final StreamCodec<FriendlyByteBuf, ClientboundClearTitlesPacket> STREAM_CODEC = Packet.codec(ClientboundClearTitlesPacket::write, ClientboundClearTitlesPacket::new);
    public ClientboundClearTitlesPacket(boolean resetTimes) {
        this.resetTimes = resetTimes;
    }
    private ClientboundClearTitlesPacket(FriendlyByteBuf input) {
        this.resetTimes = input.readBoolean();
    }
    private void write(FriendlyByteBuf output) {
        output.writeBoolean(this.resetTimes);
    }
    public PacketType<ClientboundClearTitlesPacket> type() {
        return GamePacketTypes.CLIENTBOUND_CLEAR_TITLES;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleTitlesClear(this);
    }
    public boolean shouldResetTimes() {
        return resetTimes;
    }
}
