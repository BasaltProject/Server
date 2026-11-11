// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import java.util.Optional;
import java.util.OptionalInt;
import net.minecraft.tags.FluidTags;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.levelgen.Column;
import net.minecraft.util.valueproviders.ClampedNormalFloat;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.core.Direction;
import net.minecraft.util.Mth;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.level.LevelAccessor;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderSet;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.FloatProvider;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record SpeleothemClusterFeature(BlockState baseBlock, BlockState pointedBlock, HolderSet<Block> replaceableBlocks, int floorToCeilingSearchRange, IntProvider height, IntProvider radius, int maxStalagmiteStalactiteHeightDiff, int heightDeviation, IntProvider speleothemBlockLayerThickness, FloatProvider density, FloatProvider wetness, float chanceOfSpeleothemAtMaxDistanceFromCenter, int maxDistanceFromEdgeAffectingChanceOfSpeleothem, int maxDistanceFromCenterAffectingHeightBias) implements Feature {
    private void tendColumn(WorldGenLevel level, RandomSource random, BlockPos pos, int dx, int dz, float poolOdds, double tipOdds, int clusterHeight, float dense) {
        Optional<Column> scanned = Column.scan(level, pos, floorToCeilingSearchRange, SpeleothemUtils::isEmptyOrWater, SpeleothemUtils::isNeitherEmptyNorWater);
        if (scanned.isEmpty()) {
            return;
        }
        OptionalInt ceiling = scanned.get().getCeiling();
        OptionalInt baseFloor = scanned.get().getFloor();
        if (ceiling.isEmpty() && baseFloor.isEmpty()) {
            return;
        }
        Column column = scanned.get();
        boolean pool = random.nextFloat() < poolOdds;
        if (pool && baseFloor.isPresent() && poolFits(level, pos.atY(baseFloor.getAsInt()))) {
            int y = baseFloor.getAsInt();
            column = column.withFloor(OptionalInt.of(y - 1));
            level.setBlock(pos.atY(y), Blocks.WATER.defaultBlockState(), 2);
        }
        OptionalInt floor = column.getFloor();
        int hangHeight = 0;
        if (random.nextDouble() < tipOdds && ceiling.isPresent() && !lavaAt(level, pos.atY(ceiling.getAsInt()))) {
            int thickness = speleothemBlockLayerThickness.sample(random);
            coat(level, pos.atY(ceiling.getAsInt()), thickness, Direction.UP);
            int cap = floor.isPresent() ? Math.min(clusterHeight, ceiling.getAsInt() - floor.getAsInt()) : clusterHeight;
            hangHeight = heightFor(random, dx, dz, dense, cap);
        }
        int riseHeight = 0;
        if (random.nextDouble() < tipOdds && floor.isPresent() && !lavaAt(level, pos.atY(floor.getAsInt()))) {
            int thickness = speleothemBlockLayerThickness.sample(random);
            coat(level, pos.atY(floor.getAsInt()), thickness, Direction.DOWN);
            riseHeight = ceiling.isPresent()
                ? Math.max(0, hangHeight + Mth.randomBetweenInclusive(random, -maxStalagmiteStalactiteHeightDiff, maxStalagmiteStalactiteHeightDiff))
                : heightFor(random, dx, dz, dense, clusterHeight);
        }
        int finalRise = riseHeight;
        int finalHang = hangHeight;
        if (ceiling.isPresent() && floor.isPresent() && ceiling.getAsInt() - hangHeight <= floor.getAsInt() + riseHeight) {
            int floorY = floor.getAsInt();
            int ceilingY = ceiling.getAsInt();
            int lowestBottom = Math.max(ceilingY - hangHeight, floorY + 1);
            int highestTop = Math.min(floorY + riseHeight, ceilingY - 1);
            int meet = Mth.randomBetweenInclusive(random, lowestBottom, highestTop + 1);
            finalHang = ceilingY - meet;
            finalRise = meet - 1 - floorY;
        }
        boolean merge = random.nextBoolean() && finalHang > 0 && finalRise > 0 && column.getHeight().isPresent()
            && finalHang + finalRise == column.getHeight().getAsInt();
        if (ceiling.isPresent()) {
            SpeleothemUtils.growSpeleothem(level, pos.atY(ceiling.getAsInt() - 1), Direction.DOWN, finalHang, merge, baseBlock.getBlock(), pointedBlock.getBlock(), replaceableBlocks);
        }
        if (floor.isPresent()) {
            SpeleothemUtils.growSpeleothem(level, pos.atY(floor.getAsInt() + 1), Direction.UP, finalRise, merge, baseBlock.getBlock(), pointedBlock.getBlock(), replaceableBlocks);
        }
    }
    private static boolean lavaAt(LevelReader level, BlockPos pos) {
        return level.getBlockState(pos).is(Blocks.LAVA);
    }
    private int heightFor(RandomSource random, int dx, int dz, float dense, int cap) {
        if (random.nextFloat() > dense) {
            return 0;
        }
        int dist = Math.abs(dx) + Math.abs(dz);
        float mean = (float) Mth.clampedMap((double) dist, 0.0D, (double) maxDistanceFromCenterAffectingHeightBias, (double) cap / 2.0D, 0.0D);
        return (int) ClampedNormalFloat.sample(random, mean, (float) heightDeviation, 0.0F, (float) cap);
    }
    private boolean poolFits(WorldGenLevel level, BlockPos pos) {
        BlockState state = level.getBlockState(pos);
        if (state.is(Blocks.WATER) || state.is(baseBlock.getBlock()) || state.is(pointedBlock.getBlock())) {
            return false;
        }
        if (level.getBlockState(pos.above()).getFluidState().is(FluidTags.WATER)) {
            return false;
        }
        for (Direction side : Direction.Plane.HORIZONTAL) {
            if (!wetNeighbour(level, pos.relative(side))) {
                return false;
            }
        }
        return wetNeighbour(level, pos.below());
    }
    private boolean wetNeighbour(LevelAccessor level, BlockPos pos) {
        BlockState state = level.getBlockState(pos);
        return state.is(BlockTags.BASE_STONE_OVERWORLD) || state.getFluidState().is(FluidTags.WATER);
    }
    private void coat(WorldGenLevel level, BlockPos first, int count, Direction dir) {
        BlockPos.MutableBlockPos cursor = first.mutable();
        for (int i = 0; i < count; i++) {
            if (!SpeleothemUtils.placeBaseBlockIfPossible(level, cursor, baseBlock.getBlock(), replaceableBlocks)) {
                return;
            }
            cursor.move(dir);
        }
    }
    private double oddsAt(int rx, int rz, int dx, int dz) {
        int edge = Math.min(rx - Math.abs(dx), rz - Math.abs(dz));
        return (double) Mth.clampedMap((float) edge, 0.0F, (float) maxDistanceFromEdgeAffectingChanceOfSpeleothem, chanceOfSpeleothemAtMaxDistanceFromCenter, 1.0F);
    }


    public static final MapCodec<SpeleothemClusterFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.SpeleothemClusterFeature.class, "CODEC");
    public MapCodec<SpeleothemClusterFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        if (!SpeleothemUtils.isEmptyOrWater(level, origin)) {
            return false;
        }
        int clusterHeight = height.sample(random);
        float wet = wetness.sample(random);
        float dense = density.sample(random);
        int rx = radius.sample(random);
        int rz = radius.sample(random);
        for (int dx = -rx; dx <= rx; dx++) {
            for (int dz = -rz; dz <= rz; dz++) {
                double odds = oddsAt(rx, rz, dx, dz);
                tendColumn(level, random, origin.offset(dx, 0, dz), dx, dz, wet, odds, clusterHeight, dense);
            }
        }
        return true;
    }
}
