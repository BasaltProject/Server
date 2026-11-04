// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import it.unimi.dsi.fastutil.shorts.ShortSet;
import java.util.function.BiConsumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.SectionPos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.LevelChunkSection;

public class ClientboundSectionBlocksUpdatePacket implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundSectionBlocksUpdatePacket> STREAM_CODEC =
        Packet.codec(ClientboundSectionBlocksUpdatePacket::write, ClientboundSectionBlocksUpdatePacket::new);
    private static final int kPositionBits = 12;
    private final SectionPos sectionPos;
    private final short[] positions;
    private final BlockState[] states;

    public ClientboundSectionBlocksUpdatePacket(SectionPos sectionPos, ShortSet changes, LevelChunkSection section) {
        this.sectionPos = sectionPos;
        this.positions = new short[changes.size()];
        this.states = new BlockState[changes.size()];
        int index = 0;
        for (short change : changes) {
            positions[index] = change;
            states[index] = section.getBlockState(SectionPos.sectionRelativeX(change), SectionPos.sectionRelativeY(change),
                SectionPos.sectionRelativeZ(change));
            index++;
        }
    }

    private ClientboundSectionBlocksUpdatePacket(FriendlyByteBuf input) {
        this.sectionPos = SectionPos.of(input.readLong());
        int count = input.readVarInt();
        this.positions = new short[count];
        this.states = new BlockState[count];
        for (int i = 0; i < count; i++) {
            long packed = input.readVarLong();
            positions[i] = (short) (packed & 0xFFFL);
            states[i] = Block.stateById((int) (packed >>> kPositionBits));
        }
    }

    private void write(FriendlyByteBuf output) {
        output.writeLong(sectionPos.asLong());
        output.writeVarInt(positions.length);
        for (int i = 0; i < positions.length; i++) {
            output.writeVarLong((long) Block.getId(states[i]) << kPositionBits | positions[i]);
        }
    }

    public PacketType<ClientboundSectionBlocksUpdatePacket> type() {
        return GamePacketTypes.CLIENTBOUND_SECTION_BLOCKS_UPDATE;
    }

    public void handle(ClientGamePacketListener listener) {
        listener.handleChunkBlocksUpdate(this);
    }

    public void runUpdates(BiConsumer<BlockPos, BlockState> updateFunction) {
        BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
        for (int i = 0; i < positions.length; i++) {
            short change = positions[i];
            pos.set(sectionPos.relativeToBlockX(change), sectionPos.relativeToBlockY(change), sectionPos.relativeToBlockZ(change));
            updateFunction.accept(pos, states[i]);
        }
    }
}
