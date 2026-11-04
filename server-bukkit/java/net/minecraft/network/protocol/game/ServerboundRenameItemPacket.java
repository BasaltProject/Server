// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ServerboundRenameItemPacket implements Packet<ServerGamePacketListener> {
    private final String name;
    public static final StreamCodec<FriendlyByteBuf, ServerboundRenameItemPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundRenameItemPacket", () -> Packet.codec(ServerboundRenameItemPacket::write, ServerboundRenameItemPacket::new));
    public ServerboundRenameItemPacket(String name) {
        this.name = name;
    }
    private ServerboundRenameItemPacket(FriendlyByteBuf input) {
        this.name = input.readUtf();
    }
    private void write(FriendlyByteBuf output) {
        output.writeUtf(this.name);
    }
    public PacketType<ServerboundRenameItemPacket> type() {
        return GamePacketTypes.SERVERBOUND_RENAME_ITEM;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleRenameItem(this);
    }
    public String getName() {
        return name;
    }
}
