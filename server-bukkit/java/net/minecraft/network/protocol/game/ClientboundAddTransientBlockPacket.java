// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import net.minecraft.core.BlockPos;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;

public class ClientboundAddTransientBlockPacket implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundAddTransientBlockPacket> STREAM_CODEC =
        Packet.codec(ClientboundAddTransientBlockPacket::write, ClientboundAddTransientBlockPacket::new);
    private final BlockPos pos;
    private final BlockState blockState;

    public ClientboundAddTransientBlockPacket(BlockPos pos, BlockState blockState) {
        this.pos = pos;
        this.blockState = blockState;
    }

    public ClientboundAddTransientBlockPacket(RegistryFriendlyByteBuf input) {
        this.pos = input.readBlockPos();
        this.blockState = Block.stateById(input.readVarInt());
    }

    private void write(RegistryFriendlyByteBuf output) {
        output.writeBlockPos(pos);
        output.writeVarInt(Block.getId(blockState));
    }

    public PacketType<ClientboundAddTransientBlockPacket> type() {
        return GamePacketTypes.CLIENTBOUND_ADD_TRANSIENT_BLOCK;
    }

    public void handle(ClientGamePacketListener listener) {
        listener.handleAddTransientBlockPacket(this);
    }

    public BlockState getBlockState() {
        return blockState;
    }

    public BlockPos getPos() {
        return pos;
    }
}
