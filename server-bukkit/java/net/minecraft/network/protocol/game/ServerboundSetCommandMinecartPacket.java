// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.vehicle.minecart.MinecartCommandBlock;
import org.jspecify.annotations.Nullable;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.BaseCommandBlock;
import net.minecraft.world.level.Level;
public class ServerboundSetCommandMinecartPacket implements Packet<ServerGamePacketListener> {
    private final int entity;
    private final String command;
    private final boolean trackOutput;
    public static final StreamCodec<FriendlyByteBuf, ServerboundSetCommandMinecartPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundSetCommandMinecartPacket", () -> Packet.codec(ServerboundSetCommandMinecartPacket::write, ServerboundSetCommandMinecartPacket::new));
    public ServerboundSetCommandMinecartPacket(int entity, String command, boolean trackOutput) {
        this.entity = entity;
        this.command = command;
        this.trackOutput = trackOutput;
    }
    private ServerboundSetCommandMinecartPacket(FriendlyByteBuf input) {
        this.entity = input.readVarInt();
        this.command = input.readUtf();
        this.trackOutput = input.readBoolean();
    }
    private void write(FriendlyByteBuf output) {
        output.writeVarInt(this.entity);
        output.writeUtf(this.command);
        output.writeBoolean(this.trackOutput);
    }
    public PacketType<ServerboundSetCommandMinecartPacket> type() {
        return GamePacketTypes.SERVERBOUND_SET_COMMAND_MINECART;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleSetCommandMinecart(this);
    }
    public BaseCommandBlock getCommandBlock(Level level) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerboundSetCommandMinecartPacket#getCommandBlock");
    }
    public String getCommand() {
        return command;
    }
    public boolean isTrackOutput() {
        return trackOutput;
    }
}
