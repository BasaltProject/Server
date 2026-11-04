// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import com.google.common.collect.Lists;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.util.ArrayList;
import java.util.EnumMap;
import java.util.stream.Collectors;
import net.minecraft.core.SectionPos;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.chunk.LevelChunkSection;
import org.jspecify.annotations.Nullable;

import java.util.List;
import net.minecraft.nbt.CompoundTag;
import java.util.Map;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.level.block.entity.BlockEntityType;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.levelgen.Heightmap;

public class ClientboundLevelChunkPacketData {
    private static final StreamCodec<ByteBuf, Map<Heightmap.Types, long[]>> HEIGHTMAPS = ByteBufCodecs.map(size -> new EnumMap<>(Heightmap.Types.class), Heightmap.Types.STREAM_CODEC, ByteBufCodecs.LONG_ARRAY);
    private static final int TWO_MEGABYTES = 2097152;
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundLevelChunkPacketData> STREAM_CODEC = StreamCodec.composite(
        HEIGHTMAPS, data -> data.heightmaps,
        ByteBufCodecs.byteArray(TWO_MEGABYTES), data -> data.buffer,
        BlockEntityInfo.STREAM_CODEC.apply(ByteBufCodecs.list()), data -> data.blockEntitiesData,
        ClientboundLevelChunkPacketData::new);
    private final Map<Heightmap.Types, long[]> heightmaps;
    private final byte[] buffer;
    private final List<BlockEntityInfo> blockEntitiesData;

    public ClientboundLevelChunkPacketData(LevelChunk levelChunk) {
        this.heightmaps = new EnumMap<>(Heightmap.Types.class);
        for (Object entry : levelChunk.heightmaps.entrySet()) {
            Map.Entry<Heightmap.Types, Heightmap> each = (Map.Entry<Heightmap.Types, Heightmap>) entry;
            if (each.getKey().sendToClient()) {
                heightmaps.put(each.getKey(), each.getValue().getRawData().clone());
            }
        }
        ByteBuf sink = Unpooled.buffer();
        extractChunkData(new FriendlyByteBuf(sink), levelChunk);
        this.buffer = new byte[sink.readableBytes()];
        sink.readBytes(buffer);
        this.blockEntitiesData = new ArrayList<>();
        for (Object entry : levelChunk.getBlockEntities().values()) {
            blockEntitiesData.add(BlockEntityInfo.create((BlockEntity) entry));
        }
    }

    private ClientboundLevelChunkPacketData(Map<Heightmap.Types, long[]> heightmaps, byte[] buffer, List<BlockEntityInfo> blockEntitiesData) {
        this.heightmaps = heightmaps;
        this.buffer = buffer;
        this.blockEntitiesData = blockEntitiesData;
    }

    public static void extractChunkData(FriendlyByteBuf buffer, LevelChunk chunk) {
        for (LevelChunkSection section : chunk.getSections()) {
            section.write(buffer);
        }
    }

    public void forEachBlockEntityTag(int chunkX, int chunkZ, BlockEntityTagOutput output) {
        BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
        for (BlockEntityInfo data : blockEntitiesData) {
            pos.set(chunkX * 16 + SectionPos.sectionRelative(data.packedXZ >> 4), data.y, chunkZ * 16 + SectionPos.sectionRelative(data.packedXZ));
            output.accept(pos, data.type, data.tag.orElse(null));
        }
    }

    public FriendlyByteBuf getReadBuffer() {
        return new FriendlyByteBuf(Unpooled.wrappedBuffer(buffer));
    }

    public Map<Heightmap.Types, long[]> getHeightmaps() {
        return heightmaps;
    }

    static record BlockEntityInfo(byte packedXZ, short y, BlockEntityType<?> type, Optional<CompoundTag> tag) {
        public static final StreamCodec<RegistryFriendlyByteBuf, BlockEntityInfo> STREAM_CODEC = StreamCodec.composite(ByteBufCodecs.BYTE, BlockEntityInfo::packedXZ, ByteBufCodecs.SHORT, BlockEntityInfo::y, ByteBufCodecs.registry(Registries.BLOCK_ENTITY_TYPE), BlockEntityInfo::type, ByteBufCodecs.OPTIONAL_COMPOUND_TAG, BlockEntityInfo::tag, BlockEntityInfo::new);

        static BlockEntityInfo create(BlockEntity blockEntity) {
            CompoundTag tag = blockEntity.getUpdateTag(blockEntity.getLevel().registryAccess());
            BlockPos pos = blockEntity.getBlockPos();
            int xz = SectionPos.sectionRelative(pos.getX()) << 4 | SectionPos.sectionRelative(pos.getZ());
            return new BlockEntityInfo((byte) xz, (short) pos.getY(), blockEntity.getType(), tag.isEmpty() ? Optional.empty() : Optional.of(tag));
        }
    }

    public interface BlockEntityTagOutput {
        void accept(BlockPos pos, BlockEntityType<?> type, CompoundTag tag);
    }
}
