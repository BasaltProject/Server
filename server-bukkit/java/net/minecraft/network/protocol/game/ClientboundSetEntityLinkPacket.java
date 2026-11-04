// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import org.jspecify.annotations.Nullable;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.entity.Entity;
public class ClientboundSetEntityLinkPacket implements Packet<ClientGamePacketListener> {
    private final int sourceId;
    private final int destId;
    public static final StreamCodec<FriendlyByteBuf, ClientboundSetEntityLinkPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundSetEntityLinkPacket", () -> Packet.codec(ClientboundSetEntityLinkPacket::write, ClientboundSetEntityLinkPacket::new));
    public ClientboundSetEntityLinkPacket(Entity sourceEntity, Entity destEntity) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundSetEntityLinkPacket#<init>");
    }
    private ClientboundSetEntityLinkPacket(FriendlyByteBuf input) {
        this.sourceId = input.readInt();
        this.destId = input.readInt();
    }
    private void write(FriendlyByteBuf output) {
        output.writeInt(this.sourceId);
        output.writeInt(this.destId);
    }
    public PacketType<ClientboundSetEntityLinkPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_ENTITY_LINK;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleEntityLinkPacket(this);
    }
    public int getSourceId() {
        return sourceId;
    }
    public int getDestId() {
        return destId;
    }
}
