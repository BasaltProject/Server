// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.foliageplacers;
import net.minecraft.core.Direction;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
import com.mojang.datafixers.Products;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
public abstract class FoliagePlacer {
    private static boolean extendHanging(WorldGenLevel level, FoliageSetter setter, RandomSource random, TreeFeature tree, float chance, BlockPos log, BlockPos.MutableBlockPos pos) {
        if (pos.distManhattan(log) >= 7 || random.nextFloat() > chance) {
            return false;
        }
        return tryPlaceLeaf(level, setter, random, tree, pos);
    }
    public static final Codec<FoliagePlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.foliageplacers.FoliagePlacer.class, "CODEC");
    protected final IntProvider radius;
    protected final IntProvider offset;
    protected static <P extends FoliagePlacer> Products.P2<RecordCodecBuilder.Mu<P>, IntProvider, IntProvider> foliagePlacerParts(RecordCodecBuilder.Instance<P> instance) {
        return instance.group(net.minecraft.util.valueproviders.IntProviders.codec(0, 16).fieldOf("radius").forGetter(p -> p.radius),
            net.minecraft.util.valueproviders.IntProviders.codec(0, 16).fieldOf("offset").forGetter(p -> p.offset));
    }
    public FoliagePlacer(IntProvider radius, IntProvider offset) {
        this.radius = radius;
        this.offset = offset;
    }
    protected abstract FoliagePlacerType<?> type();
    public void createFoliage(WorldGenLevel level, FoliageSetter foliageSetter, RandomSource random, TreeFeature tree, int treeHeight, FoliageAttachment foliageAttachment, int foliageHeight, int leafRadius) {
        createFoliage(level, foliageSetter, random, tree, treeHeight, foliageAttachment, foliageHeight, leafRadius, offset.sample(random));
    }
    protected abstract void createFoliage(WorldGenLevel level, FoliageSetter foliageSetter, RandomSource random, TreeFeature tree, int treeHeight, FoliageAttachment foliageAttachment, int foliageHeight, int leafRadius, int offset);
    public abstract int foliageHeight(RandomSource random, int treeHeight, TreeFeature tree);
    public int foliageRadius(RandomSource random, int trunkHeight) {
        return radius.sample(random);
    }
    protected abstract boolean shouldSkipLocation(RandomSource random, int dx, int y, int dz, int currentRadius, boolean doubleTrunk);
    protected boolean shouldSkipLocationSigned(RandomSource random, int dx, int y, int dz, int currentRadius, boolean doubleTrunk) {
        int nearX = Math.abs(dx);
        int nearZ = Math.abs(dz);
        if (doubleTrunk) {
            nearX = Math.min(nearX, Math.abs(dx - 1));
            nearZ = Math.min(nearZ, Math.abs(dz - 1));
        }
        return shouldSkipLocation(random, nearX, y, nearZ, currentRadius, doubleTrunk);
    }
    protected void placeLeavesRow(WorldGenLevel level, FoliageSetter foliageSetter, RandomSource random, TreeFeature tree, BlockPos origin, int currentRadius, int y, boolean doubleTrunk) {
        int extra = doubleTrunk ? 1 : 0;
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int dx = -currentRadius; dx <= currentRadius + extra; dx++) {
            for (int dz = -currentRadius; dz <= currentRadius + extra; dz++) {
                if (!shouldSkipLocationSigned(random, dx, y, dz, currentRadius, doubleTrunk)) {
                    cursor.setWithOffset(origin, dx, y, dz);
                    tryPlaceLeaf(level, foliageSetter, random, tree, cursor);
                }
            }
        }
    }
    protected final void placeLeavesRowWithHangingLeavesBelow(WorldGenLevel level, FoliageSetter foliageSetter, RandomSource random, TreeFeature tree, BlockPos origin, int currentRadius, int y, boolean doubleTrunk, float hangingLeavesChance, float hangingLeavesExtensionChance) {
        placeLeavesRow(level, foliageSetter, random, tree, origin, currentRadius, y, doubleTrunk);
        int extra = doubleTrunk ? 1 : 0;
        BlockPos log = origin.below();
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (Direction along : Direction.Plane.HORIZONTAL) {
            Direction toEdge = along.getClockWise();
            int edgeOffset = toEdge.getAxisDirection() == Direction.AxisDirection.POSITIVE ? currentRadius + extra : currentRadius;
            cursor.setWithOffset(origin, 0, y - 1, 0).move(toEdge, edgeOffset).move(along, -currentRadius);
            for (int slide = -currentRadius; slide < currentRadius + extra; slide++) {
                boolean roofed = foliageSetter.isSet(cursor.move(Direction.UP));
                cursor.move(Direction.DOWN);
                if (roofed && extendHanging(level, foliageSetter, random, tree, hangingLeavesChance, log, cursor)) {
                    cursor.move(Direction.DOWN);
                    extendHanging(level, foliageSetter, random, tree, hangingLeavesExtensionChance, log, cursor);
                    cursor.move(Direction.UP);
                }
                cursor.move(along);
            }
        }
    }
    protected static boolean tryPlaceLeaf(WorldGenLevel level, FoliageSetter foliageSetter, RandomSource random, TreeFeature tree, BlockPos pos) {
        boolean persistent = level.isStateAtPosition(pos, state -> state.getValueOrElse(net.minecraft.world.level.block.state.properties.BlockStateProperties.PERSISTENT, false));
        if (persistent || !TreeFeature.validTreePos(level, pos)) {
            return false;
        }
        BlockState leaf = ((BlockStateProvider) tree.foliageProvider().value()).getState(level, random, pos);
        if (leaf.hasProperty(net.minecraft.world.level.block.state.properties.BlockStateProperties.WATERLOGGED)) {
            boolean inWater = level.isFluidAtPosition(pos, fluid -> fluid.isSourceOfType(net.minecraft.world.level.material.Fluids.WATER));
            leaf = leaf.setValue(net.minecraft.world.level.block.state.properties.BlockStateProperties.WATERLOGGED, inWater);
        }
        foliageSetter.set(pos, leaf);
        return true;
    }
    public static record FoliageAttachment(BlockPos pos, int radiusOffsetXZ, int foliageHeightOffset, int sizeX, int sizeZ) {
        public FoliageAttachment(BlockPos pos, int radiusOffsetXZ, boolean doubleTrunk) {
            this(pos, radiusOffsetXZ, doubleTrunk ? 2 : 1, doubleTrunk ? 2 : 1);
        }
        public FoliageAttachment(BlockPos pos, int radiusOffsetXZ, int sizeX, int sizeZ) {
            this(pos, radiusOffsetXZ, 0, sizeX, sizeZ);
        }
        public boolean doubleTrunk() {
            return sizeX == 2 && sizeZ == 2;
        }
    }
    public interface FoliageSetter {
        void set(BlockPos pos, BlockState state);
        boolean isSet(BlockPos pos);
    }
}
