// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import net.minecraft.util.RandomSource;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelReader;
public class RuleProcessor implements StructureProcessor {
    private final List<ProcessorRule> rules;
    public static final MapCodec<RuleProcessor> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.RuleProcessor.class, "MAP_CODEC");
    public RuleProcessor(List<? extends ProcessorRule> rules) {
        this.rules = List.copyOf(rules);
    }
    public StructureTemplate. StructureBlockInfo processBlock(LevelReader level, BlockPos targetPosition, BlockPos referencePos, BlockPos templateRelativePos, StructureTemplate.StructureBlockInfo processedBlockInfo, StructurePlaceSettings settings) {
        RandomSource random = RandomSource.create(net.minecraft.util.Mth.getSeed(processedBlockInfo.pos()));
        for (ProcessorRule rule : rules) {
            if (rule.test(level, processedBlockInfo.state(), templateRelativePos, processedBlockInfo.pos(), referencePos, random)) {
                return new StructureTemplate.StructureBlockInfo(processedBlockInfo.pos(), rule.getOutputState(), rule.getOutputTag(random, processedBlockInfo.nbt()));
            }
        }
        return processedBlockInfo;
    }
    public MapCodec<RuleProcessor> codec() {
        return MAP_CODEC;
    }
}
