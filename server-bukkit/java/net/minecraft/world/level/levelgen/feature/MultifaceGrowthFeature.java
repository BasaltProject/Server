// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import it.unimi.dsi.fastutil.objects.ObjectArrayList;
import net.minecraft.world.level.block.Blocks;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.HolderSet;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.MultifaceSpreadeableBlock;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record MultifaceGrowthFeature(Block placeBlock, int searchRange, boolean canPlaceOnFloor, boolean canPlaceOnCeiling, boolean canPlaceOnWall, float chanceOfSpreading, HolderSet<Block> canBePlacedOn) implements Feature {
    private static boolean wetOrEmpty(BlockState state) {
        return state.isAir() || state.is(Blocks.WATER);
    }
    private ObjectArrayList<Direction> faces() {
        ObjectArrayList<Direction> out = new ObjectArrayList<>(6);
        if (canPlaceOnCeiling) {
            out.add(Direction.UP);
        }
        if (canPlaceOnFloor) {
            out.add(Direction.DOWN);
        }
        if (canPlaceOnWall) {
            Direction.Plane.HORIZONTAL.forEach(out::add);
        }
        return out;
    }
    private List<Direction> facesExcept(RandomSource random, Direction skip) {
        ObjectArrayList<Direction> out = faces();
        out.remove(skip);
        net.minecraft.util.Util.shuffle(out, random);
        return out;
    }
    public static final MapCodec<MultifaceGrowthFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.MultifaceGrowthFeature.class, "CODEC");
    public MapCodec<MultifaceGrowthFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        if (!wetOrEmpty(level.getBlockState(origin))) {
            return false;
        }
        if (!(placeBlock instanceof MultifaceSpreadeableBlock grower)) {
            return false;
        }
        List<Direction> order = net.minecraft.util.Util.shuffledCopy(faces(), random);
        if (placeGrowthIfPossible(grower, level, origin, level.getBlockState(origin), random, order)) {
            return true;
        }
        BlockPos.MutableBlockPos cursor = origin.mutable();
        for (Direction heading : order) {
            cursor.set(origin);
            List<Direction> options = facesExcept(random, heading.getOpposite());
            for (int i = 0; i < searchRange; i++) {
                cursor.setWithOffset(origin, heading);
                BlockState found = level.getBlockState(cursor);
                if (!wetOrEmpty(found) && !found.is(placeBlock)) {
                    break;
                }
                if (placeGrowthIfPossible(grower, level, cursor, found, random, options)) {
                    return true;
                }
            }
        }
        return false;
    }
    public boolean placeGrowthIfPossible(MultifaceSpreadeableBlock placerBlock, WorldGenLevel level, BlockPos pos, BlockState oldState, RandomSource random, List<Direction> placementDirections) {
        BlockPos.MutableBlockPos probe = pos.mutable();
        for (Direction face : placementDirections) {
            if (!level.getBlockState(probe.setWithOffset(pos, face)).is(canBePlacedOn)) {
                continue;
            }
            BlockState made = placerBlock.getStateForPlacement(oldState, level, pos, face);
            if (made == null) {
                return false;
            }
            level.setBlockAndUpdate(pos, made);
            level.getChunk(pos).markPosForPostProcessing(pos);
            if (random.nextFloat() < chanceOfSpreading) {
                placerBlock.getSpreader().spreadFromFaceTowardRandomDirection(made, level, pos, face, random, true);
            }
            return true;
        }
        return false;


    }
}
