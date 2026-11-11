// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.util.RandomSource;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelReader;
public class BlockAgeProcessor implements StructureProcessor {
    private final float mossiness;
    private static final BlockState[] kNonMossy = {net.minecraft.world.level.block.Blocks.STONE_SLAB.defaultBlockState(),
        net.minecraft.world.level.block.Blocks.STONE_BRICK_SLAB.defaultBlockState()};
    private static BlockState randomStairs(RandomSource random, net.minecraft.world.level.block.Block stairs) {
        BlockState state = stairs.defaultBlockState().setValue(net.minecraft.world.level.block.StairBlock.FACING,
            net.minecraft.core.Direction.Plane.HORIZONTAL.getRandomDirection(random));
        net.minecraft.world.level.block.state.properties.Half[] halves = net.minecraft.world.level.block.state.properties.Half.values();
        return state.setValue(net.minecraft.world.level.block.StairBlock.HALF, halves[random.nextInt(halves.length)]);
    }
    private BlockState pick(RandomSource random, BlockState[] plain, BlockState[] mossy) {
        BlockState[] from = random.nextFloat() < mossiness ? mossy : plain;
        return from[random.nextInt(from.length)];
    }
    public static final MapCodec<BlockAgeProcessor> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.BlockAgeProcessor.class, "MAP_CODEC");
    public BlockAgeProcessor(float mossiness) {
        this.mossiness = mossiness;
    }
    public StructureTemplate. StructureBlockInfo processBlock(LevelReader level, BlockPos targetPosition, BlockPos referencePos, BlockPos templateRelativePos, StructureTemplate.StructureBlockInfo processedBlockInfo, StructurePlaceSettings settings) {
        RandomSource random = settings.getRandom(processedBlockInfo.pos());
        BlockState state = processedBlockInfo.state();
        BlockState replacement = null;
        if (state.is(Blocks.STONE_BRICKS) || state.is(Blocks.STONE) || state.is(Blocks.CHISELED_STONE_BRICKS)) {
            if (random.nextFloat() < 0.5F) {
                BlockState[] plain = {Blocks.CRACKED_STONE_BRICKS.defaultBlockState(), randomStairs(random, Blocks.STONE_BRICK_STAIRS)};
                BlockState[] mossy = {Blocks.MOSSY_STONE_BRICKS.defaultBlockState(), randomStairs(random, Blocks.MOSSY_STONE_BRICK_STAIRS)};
                replacement = pick(random, plain, mossy);
            }
        } else if (state.is(net.minecraft.tags.BlockTags.STAIRS)) {
            if (random.nextFloat() < 0.5F) {
                BlockState[] mossy = {Blocks.MOSSY_STONE_BRICK_STAIRS.withPropertiesOf(state), Blocks.MOSSY_STONE_BRICK_SLAB.defaultBlockState()};
                replacement = pick(random, kNonMossy, mossy);
            }
        } else if (state.is(net.minecraft.tags.BlockTags.SLABS)) {
            replacement = random.nextFloat() < mossiness ? Blocks.MOSSY_STONE_BRICK_SLAB.withPropertiesOf(state) : null;
        } else if (state.is(net.minecraft.tags.BlockTags.WALLS)) {
            replacement = random.nextFloat() < mossiness ? Blocks.MOSSY_STONE_BRICK_WALL.withPropertiesOf(state) : null;
        } else if (state.is(Blocks.OBSIDIAN)) {
            replacement = random.nextFloat() < 0.15F ? Blocks.CRYING_OBSIDIAN.defaultBlockState() : null;
        }
        return replacement != null ? new StructureTemplate.StructureBlockInfo(processedBlockInfo.pos(), replacement, processedBlockInfo.nbt()) : processedBlockInfo;
    }
    public MapCodec<BlockAgeProcessor> codec() {
        return MAP_CODEC;
    }
}
