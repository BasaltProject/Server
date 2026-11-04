// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import net.minecraft.core.BlockPos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.block.entity.JigsawBlockEntity;

public class ServerboundSetJigsawBlockPacket implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ServerboundSetJigsawBlockPacket> STREAM_CODEC =
        Packet.codec(ServerboundSetJigsawBlockPacket::write, ServerboundSetJigsawBlockPacket::new);
    private final BlockPos pos;
    private final Identifier name;
    private final Identifier target;
    private final Identifier pool;
    private final String finalState;
    private final JigsawBlockEntity.JointType joint;
    private final int selectionPriority;
    private final int placementPriority;

    public ServerboundSetJigsawBlockPacket(BlockPos blockPos, Identifier name, Identifier target, Identifier pool, String finalState,
        JigsawBlockEntity.JointType joint, int selectionPriority, int placementPriority) {
        this.pos = blockPos;
        this.name = name;
        this.target = target;
        this.pool = pool;
        this.finalState = finalState;
        this.joint = joint;
        this.selectionPriority = selectionPriority;
        this.placementPriority = placementPriority;
    }

    private ServerboundSetJigsawBlockPacket(FriendlyByteBuf input) {
        this.pos = input.readBlockPos();
        this.name = input.readIdentifier();
        this.target = input.readIdentifier();
        this.pool = input.readIdentifier();
        this.finalState = input.readUtf();
        String jointName = input.readUtf();
        this.joint = JigsawBlockEntity.JointType.valueOf(jointName.toUpperCase(java.util.Locale.ROOT));
        this.selectionPriority = input.readVarInt();
        this.placementPriority = input.readVarInt();
    }

    private void write(FriendlyByteBuf output) {
        output.writeBlockPos(pos);
        output.writeIdentifier(name);
        output.writeIdentifier(target);
        output.writeIdentifier(pool);
        output.writeUtf(finalState);
        output.writeUtf(joint.getSerializedName());
        output.writeVarInt(selectionPriority);
        output.writeVarInt(placementPriority);
    }

    public PacketType<ServerboundSetJigsawBlockPacket> type() {
        return GamePacketTypes.SERVERBOUND_SET_JIGSAW_BLOCK;
    }

    public void handle(ServerGamePacketListener listener) {
        listener.handleSetJigsawBlock(this);
    }

    public BlockPos getPos() {
        return pos;
    }

    public Identifier getName() {
        return name;
    }

    public Identifier getTarget() {
        return target;
    }

    public Identifier getPool() {
        return pool;
    }

    public String getFinalState() {
        return finalState;
    }

    public JigsawBlockEntity.JointType getJoint() {
        return joint;
    }

    public int getSelectionPriority() {
        return selectionPriority;
    }

    public int getPlacementPriority() {
        return placementPriority;
    }
}
