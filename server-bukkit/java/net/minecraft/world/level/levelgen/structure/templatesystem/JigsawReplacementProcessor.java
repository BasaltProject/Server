// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelReader;
public class JigsawReplacementProcessor implements StructureProcessor {
    public static final MapCodec<JigsawReplacementProcessor> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.JigsawReplacementProcessor.class, "MAP_CODEC");
    public static final JigsawReplacementProcessor INSTANCE = new JigsawReplacementProcessor();
    private JigsawReplacementProcessor() {
        
    }
    public StructureTemplate. StructureBlockInfo processBlock(LevelReader level, BlockPos targetPosition, BlockPos referencePos, BlockPos templateRelativePos, StructureTemplate.StructureBlockInfo processedBlockInfo, StructurePlaceSettings settings) {
        BlockState state = processedBlockInfo.state();
        if (!state.is(Blocks.JIGSAW)) {
            return processedBlockInfo;
        }
        if (processedBlockInfo.nbt() == null) {
            return processedBlockInfo;
        }
        String text = processedBlockInfo.nbt().getStringOr("final_state", "minecraft:air");
        BlockState parsed;
        try {
            parsed = net.minecraft.commands.arguments.blocks.BlockStateParser.parseForBlock(level.holderLookup(net.minecraft.core.registries.Registries.BLOCK), text, true).blockState();
        } catch (com.mojang.brigadier.exceptions.CommandSyntaxException failure) {
            return null;
        }
        return parsed.is(Blocks.STRUCTURE_VOID) ? null : new StructureTemplate.StructureBlockInfo(processedBlockInfo.pos(), parsed, null);
    }
    public MapCodec<JigsawReplacementProcessor> codec() {
        return MAP_CODEC;
    }
}
