// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import net.minecraft.core.BlockPos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.block.entity.CommandBlockEntity;

public class ServerboundSetCommandBlockPacket implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ServerboundSetCommandBlockPacket> STREAM_CODEC =
        Packet.codec(ServerboundSetCommandBlockPacket::write, ServerboundSetCommandBlockPacket::new);
    private final BlockPos pos;
    private final String command;
    private final boolean trackOutput;
    private final boolean conditional;
    private final boolean automatic;
    private final CommandBlockEntity.Mode mode;

    public ServerboundSetCommandBlockPacket(BlockPos pos, String command, CommandBlockEntity.Mode mode, boolean trackOutput,
        boolean conditional, boolean automatic) {
        this.pos = pos;
        this.command = command;
        this.trackOutput = trackOutput;
        this.conditional = conditional;
        this.automatic = automatic;
        this.mode = mode;
    }

    private ServerboundSetCommandBlockPacket(FriendlyByteBuf input) {
        this.pos = input.readBlockPos();
        this.command = input.readUtf();
        this.mode = input.readEnum(CommandBlockEntity.Mode.class);
        int flags = input.readByte();
        this.trackOutput = (flags & 1) != 0;
        this.conditional = (flags & 2) != 0;
        this.automatic = (flags & 4) != 0;
    }

    private void write(FriendlyByteBuf output) {
        output.writeBlockPos(pos);
        output.writeUtf(command);
        output.writeEnum(mode);
        output.writeByte((trackOutput ? 1 : 0) | (conditional ? 2 : 0) | (automatic ? 4 : 0));
    }

    public PacketType<ServerboundSetCommandBlockPacket> type() {
        return GamePacketTypes.SERVERBOUND_SET_COMMAND_BLOCK;
    }

    public void handle(ServerGamePacketListener listener) {
        listener.handleSetCommandBlock(this);
    }

    public BlockPos getPos() {
        return pos;
    }

    public String getCommand() {
        return command;
    }

    public boolean isTrackOutput() {
        return trackOutput;
    }

    public boolean isConditional() {
        return conditional;
    }

    public boolean isAutomatic() {
        return automatic;
    }

    public CommandBlockEntity.Mode getMode() {
        return mode;
    }
}
