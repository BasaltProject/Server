// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;

public class ServerboundPlayerActionPacket implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ServerboundPlayerActionPacket> STREAM_CODEC =
        Packet.codec(ServerboundPlayerActionPacket::write, ServerboundPlayerActionPacket::new);
    private final BlockPos pos;
    private final Direction direction;
    private final Action action;
    private final int sequence;

    public ServerboundPlayerActionPacket(Action action, BlockPos pos, Direction direction, int sequence) {
        this.action = action;
        this.pos = pos.immutable();
        this.direction = direction;
        this.sequence = sequence;
    }

    public ServerboundPlayerActionPacket(Action action, BlockPos pos, Direction direction) {
        this(action, pos, direction, 0);
    }

    private ServerboundPlayerActionPacket(FriendlyByteBuf input) {
        this.action = input.readEnum(Action.class);
        this.pos = input.readBlockPos();
        this.direction = Direction.from3DDataValue(input.readUnsignedByte());
        this.sequence = input.readVarInt();
    }

    private void write(FriendlyByteBuf output) {
        output.writeEnum(action);
        output.writeBlockPos(pos);
        output.writeByte(direction.get3DDataValue());
        output.writeVarInt(sequence);
    }

    public PacketType<ServerboundPlayerActionPacket> type() {
        return GamePacketTypes.SERVERBOUND_PLAYER_ACTION;
    }

    public void handle(ServerGamePacketListener listener) {
        listener.handlePlayerAction(this);
    }

    public BlockPos getPos() {
        return pos;
    }

    public Direction getDirection() {
        return direction;
    }

    public Action getAction() {
        return action;
    }

    public int getSequence() {
        return sequence;
    }

    public static enum Action {
        START_DESTROY_BLOCK, CHANGE_DESTROY_DIRECTION, ABORT_DESTROY_BLOCK, STOP_DESTROY_BLOCK, DROP_ALL_ITEMS, DROP_ITEM,
        RELEASE_USE_ITEM, SWAP_ITEM_WITH_OFFHAND, STAB;
    }
}
