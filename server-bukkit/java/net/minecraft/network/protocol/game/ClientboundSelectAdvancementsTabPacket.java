// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.resources.Identifier;
public class ClientboundSelectAdvancementsTabPacket implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundSelectAdvancementsTabPacket> STREAM_CODEC = Packet.codec(ClientboundSelectAdvancementsTabPacket::write, ClientboundSelectAdvancementsTabPacket::new);
    private final Identifier tab;
    public ClientboundSelectAdvancementsTabPacket(Identifier tab) {
        this.tab = tab;
    }
    private ClientboundSelectAdvancementsTabPacket(FriendlyByteBuf input) {
        this.tab = input.readNullable(FriendlyByteBuf::readIdentifier);
    }
    private void write(FriendlyByteBuf output) {
        output.writeNullable(tab, FriendlyByteBuf::writeIdentifier);
    }
    public PacketType<ClientboundSelectAdvancementsTabPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SELECT_ADVANCEMENTS_TAB;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSelectAdvancementsTab(this);
    }
    public Identifier getTab() {
        return tab;
    }
}
