// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ServerboundClientCommandPacket implements Packet<ServerGamePacketListener> {
    private final Action action;
    public static final StreamCodec<FriendlyByteBuf, ServerboundClientCommandPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundClientCommandPacket", () -> Packet.codec(ServerboundClientCommandPacket::write, ServerboundClientCommandPacket::new));
    public ServerboundClientCommandPacket(Action action) {
        this.action = action;
    }
    private ServerboundClientCommandPacket(FriendlyByteBuf input) {
        this.action = input.readEnum(Action.class);
    }
    private void write(FriendlyByteBuf output) {
        output.writeEnum(this.action);
    }
    public PacketType<ServerboundClientCommandPacket> type() {
        return GamePacketTypes.SERVERBOUND_CLIENT_COMMAND;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleClientCommand(this);
    }
    public Action getAction() {
        return action;
    }
    public static enum Action {
        PERFORM_RESPAWN, REQUEST_STATS, REQUEST_GAMERULE_VALUES;
    }
}
