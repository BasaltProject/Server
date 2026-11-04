// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.common;
import java.util.UUID;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ServerboundResourcePackPacket(UUID id, Action action) implements Packet<ServerCommonPacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ServerboundResourcePackPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundResourcePackPacket", () -> StreamCodec.of((output, packet) -> {
            output.writeUUID(packet.id());
            output.writeEnum(packet.action());
        }, input -> new ServerboundResourcePackPacket(input.readUUID(), input.readEnum(Action.class))));
    private ServerboundResourcePackPacket(FriendlyByteBuf input) {
        this((UUID) null, (Action) null); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.common.ServerboundResourcePackPacket#<init>");
    }
    public PacketType<ServerboundResourcePackPacket> type() {
        return CommonPacketTypes.SERVERBOUND_RESOURCE_PACK;
    }
    public void handle(ServerCommonPacketListener listener) {
        listener.handleResourcePackResponse(this);
    }
    public static enum Action {
        SUCCESSFULLY_LOADED, DECLINED, FAILED_DOWNLOAD, ACCEPTED, DOWNLOADED, INVALID_URL, FAILED_RELOAD, DISCARDED;
        public boolean isTerminal() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.common.ServerboundResourcePackPacket.Action#isTerminal");
        }
    }
}
