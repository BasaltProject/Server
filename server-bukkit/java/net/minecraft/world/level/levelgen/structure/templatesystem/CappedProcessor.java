// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import net.minecraft.util.RandomSource;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.ServerLevelAccessor;
public class CappedProcessor implements StructureProcessor {
    private final StructureProcessor delegate;
    private final IntProvider limit;
    public static final MapCodec<CappedProcessor> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.CappedProcessor.class, "MAP_CODEC");
    public CappedProcessor(StructureProcessor delegate, IntProvider limit) {
        this.delegate = delegate;
        this.limit = limit;
    }
    public MapCodec<CappedProcessor> codec() {
        return MAP_CODEC;
    }
    public boolean evaluatesEntirePieceState() {
        return true;
    }
    public final List<StructureTemplate.StructureBlockInfo> finalizeProcessing(ServerLevelAccessor level, BlockPos position, BlockPos referencePos, List<StructureTemplate.StructureBlockInfo> originalBlockInfoList, List<StructureTemplate.StructureBlockInfo> processedBlockInfoList, StructurePlaceSettings settings) {
        if (limit.maxInclusive() == 0 || processedBlockInfoList.isEmpty() || originalBlockInfoList.size() != processedBlockInfoList.size()) {
            return processedBlockInfoList;
        }
        RandomSource random = RandomSource.createThreadLocalInstance(level.getLevel().getSeed()).forkPositional().at(position);
        int budget = Math.min(limit.sample(random), processedBlockInfoList.size());
        if (budget < 1) {
            return processedBlockInfoList;
        }
        List<Integer> order = new java.util.ArrayList<>();
        for (int at = 0; at < processedBlockInfoList.size(); at++) {
            order.add(at);
        }
        for (int at = order.size(); at > 1; at--) {
            java.util.Collections.swap(order, at - 1, random.nextInt(at));
        }
        int replaced = 0;
        for (int index : order) {
            if (replaced >= budget) {
                break;
            }
            StructureTemplate.StructureBlockInfo processed = processedBlockInfoList.get(index);
            StructureTemplate.StructureBlockInfo altered = delegate.processBlock(level, position, referencePos, originalBlockInfoList.get(index).pos(), processed, settings);
            if (altered != null && !processed.equals(altered)) {
                replaced++;
                processedBlockInfoList.set(index, altered);
            }
        }
        return processedBlockInfoList;
    }
}
