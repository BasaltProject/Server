// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import java.util.Optional;
import net.minecraft.core.Direction;
import net.minecraft.world.level.LevelAccessor;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderSet;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record SpeleothemFeature(BlockState baseBlock, BlockState pointedBlock, HolderSet<Block> replaceableBlocks, float chanceOfTallerGeneration, float chanceOfDirectionalSpread, float chanceOfSpreadRadius2, float chanceOfSpreadRadius3) implements Feature {
    private Optional<Direction> pickTip(LevelAccessor level, BlockPos pos, RandomSource random) {
        boolean above = SpeleothemUtils.isBase(level.getBlockState(pos.above()), baseBlock.getBlock(), replaceableBlocks);
        boolean below = SpeleothemUtils.isBase(level.getBlockState(pos.below()), baseBlock.getBlock(), replaceableBlocks);
        if (above && below) {
            return Optional.of(random.nextBoolean() ? Direction.DOWN : Direction.UP);
        }
        if (above) {
            return Optional.of(Direction.DOWN);
        }
        return below ? Optional.of(Direction.UP) : Optional.empty();
    }
    private void putBase(LevelAccessor level, BlockPos pos) {
        SpeleothemUtils.placeBaseBlockIfPossible(level, pos, baseBlock.getBlock(), replaceableBlocks);
    }
    private void scatterBase(LevelAccessor level, RandomSource random, BlockPos root) {
        putBase(level, root);
        for (Direction side : Direction.Plane.HORIZONTAL) {
            if (random.nextFloat() > chanceOfDirectionalSpread) {
                continue;
            }
            BlockPos first = root.relative(side);
            putBase(level, first);
            if (random.nextFloat() > chanceOfSpreadRadius2) {
                continue;
            }
            BlockPos second = first.relative(Direction.getRandom(random));
            putBase(level, second);
            if (random.nextFloat() <= chanceOfSpreadRadius3) {
                putBase(level, second.relative(Direction.getRandom(random)));
            }
        }
    }
    public static final MapCodec<SpeleothemFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.SpeleothemFeature.class, "CODEC");
    public MapCodec<SpeleothemFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        Optional<Direction> tip = pickTip(level, origin, random);
        if (tip.isEmpty()) {
            return false;
        }
        Direction dir = tip.get();
        scatterBase(level, random, origin.relative(dir.getOpposite()));
        boolean taller = random.nextFloat() < chanceOfTallerGeneration && SpeleothemUtils.isEmptyOrWater(level.getBlockState(origin.relative(dir)));
        SpeleothemUtils.growSpeleothem(level, origin, dir, taller ? 2 : 1, false, baseBlock.getBlock(), pointedBlock.getBlock(), replaceableBlocks);
        return true;
    }
}
