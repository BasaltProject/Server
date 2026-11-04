// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.entity.Entity;
public class ServerboundPlayerCommandPacket implements Packet<ServerGamePacketListener> {
    private final int id;
    private final Action action;
    private final int data;
    public static final StreamCodec<FriendlyByteBuf, ServerboundPlayerCommandPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundPlayerCommandPacket", () -> Packet.codec(ServerboundPlayerCommandPacket::write, ServerboundPlayerCommandPacket::new));
    public ServerboundPlayerCommandPacket(Entity entity, Action action) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerboundPlayerCommandPacket#<init>");
    }
    public ServerboundPlayerCommandPacket(Entity entity, Action action, int data) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerboundPlayerCommandPacket#<init>");
    }
    private ServerboundPlayerCommandPacket(FriendlyByteBuf input) {
        this.id = input.readVarInt();
        this.action = input.readEnum(Action.class);
        this.data = input.readVarInt();
    }
    private void write(FriendlyByteBuf output) {
        output.writeVarInt(this.id);
        output.writeEnum(this.action);
        output.writeVarInt(this.data);
    }
    public PacketType<ServerboundPlayerCommandPacket> type() {
        return GamePacketTypes.SERVERBOUND_PLAYER_COMMAND;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handlePlayerCommand(this);
    }
    public int getId() {
        return id;
    }
    public Action getAction() {
        return action;
    }
    public int getData() {
        return data;
    }
    public static enum Action {
        STOP_SLEEPING, START_SPRINTING, STOP_SPRINTING, START_RIDING_JUMP, STOP_RIDING_JUMP, OPEN_INVENTORY, START_FALL_FLYING;
    }
}
