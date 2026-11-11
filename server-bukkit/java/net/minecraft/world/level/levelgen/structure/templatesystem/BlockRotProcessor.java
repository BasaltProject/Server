// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import net.minecraft.util.RandomSource;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.Block;
public class BlockRotProcessor implements StructureProcessor {
    private final java.util.Optional<HolderSet<Block>> rottableBlocks;
    private final float integrity;
    public static final MapCodec<BlockRotProcessor> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.BlockRotProcessor.class, "MAP_CODEC");
    public BlockRotProcessor(HolderSet<Block> tag, float integrity) {
        this(Optional.of(tag), integrity);
    }
    public BlockRotProcessor(float integrity) {
        this(Optional.empty(), integrity);
    }
    private BlockRotProcessor(Optional<HolderSet<Block>> blockTagKey, float integrity) {
        this.integrity = integrity;
        this.rottableBlocks = blockTagKey;
    }
    public StructureTemplate. StructureBlockInfo processBlock(LevelReader level, BlockPos targetPosition, BlockPos referencePos, BlockPos templateRelativePos, StructureTemplate.StructureBlockInfo processedBlockInfo, StructurePlaceSettings settings) {
        RandomSource random = settings.getRandom(processedBlockInfo.pos());
        boolean rottable = rottableBlocks.isEmpty() || processedBlockInfo.state().is(rottableBlocks.get());
        return rottable && !(random.nextFloat() <= integrity) ? null : processedBlockInfo;
    }
    public MapCodec<BlockRotProcessor> codec() {
        return MAP_CODEC;
    }
}
