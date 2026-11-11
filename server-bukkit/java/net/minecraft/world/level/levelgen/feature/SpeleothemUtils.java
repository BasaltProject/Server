// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import org.basaltmc.bukkit.facade.StateProps;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.util.Mth;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
public class SpeleothemUtils {
    private static BlockState pointed(Direction direction, String thickness, Block pointedBlock) {
        return StateProps.with(StateProps.with(pointedBlock.defaultBlockState(), "tip_direction", direction.getSerializedName()), "thickness", thickness);
    }


    protected static double getSpeleothemHeight(double xzDistanceFromCenter, double speleothemRadius, double scale, double bluntness) {
        double d = Math.max(xzDistanceFromCenter, bluntness);
        double r = d / speleothemRadius * 0.384;
        double shape = 0.75D * Math.pow(r, 4.0D / 3.0D) - Math.pow(r, 2.0D / 3.0D) - (1.0D / 3.0D) * Math.log(r);
        double relative = Math.max(scale * shape, 0.0D);
        return relative / 0.384 * speleothemRadius;
    }
    protected static boolean isCircleMostlyEmbeddedInStone(WorldGenLevel level, BlockPos center, int xzRadius) {
        if (isEmptyOrWaterOrLava(level, center)) {
            return false;
        }
        float step = 6.0F / (float) xzRadius;
        for (float angle = 0.0F; angle < ((float) Math.PI * 2F); angle += step) {
            int dx = (int) (Mth.cos((double) angle) * (float) xzRadius);
            int dz = (int) (Mth.sin((double) angle) * (float) xzRadius);
            if (isEmptyOrWaterOrLava(level, center.offset(dx, 0, dz))) {
                return false;
            }
        }
        return true;
    }
    protected static boolean isEmptyOrWater(LevelAccessor level, BlockPos pos) {
        return level.isStateAtPosition(pos, SpeleothemUtils::isEmptyOrWater);
    }
    protected static boolean isEmptyOrWaterOrLava(LevelAccessor level, BlockPos pos) {
        return level.isStateAtPosition(pos, SpeleothemUtils::isEmptyOrWaterOrLava);
    }
    protected static void buildBaseToTipColumn(Direction direction, int totalLength, boolean mergedTip, Consumer<BlockState> consumer, Block pointedBlock) {
        if (totalLength >= 3) {
            consumer.accept(pointed(direction, "base", pointedBlock));
            for (int i = 0; i < totalLength - 3; i++) {
                consumer.accept(pointed(direction, "middle", pointedBlock));
            }
        }
        if (totalLength >= 2) {
            consumer.accept(pointed(direction, "frustum", pointedBlock));
        }
        if (totalLength >= 1) {
            consumer.accept(pointed(direction, mergedTip ? "tip_merge" : "tip", pointedBlock));
        }
    }
    protected static void growSpeleothem(LevelAccessor level, BlockPos startPos, Direction tipDirection, int height, boolean mergedTip, Block baseBlock, Block pointedBlock, HolderSet<Block> replaceableBlocks) {
        if (!isBase(level.getBlockState(startPos.relative(tipDirection.getOpposite())), baseBlock, replaceableBlocks)) {
            return;
        }
        BlockPos.MutableBlockPos cursor = startPos.mutable();
        buildBaseToTipColumn(tipDirection, height, mergedTip, state -> {
            BlockState placed = state.is(pointedBlock) ? StateProps.with(state, "waterlogged", level.isWaterAt(cursor)) : state;
            level.setBlock(cursor, placed, 2);
            cursor.move(tipDirection);
        }, pointedBlock);
    }
    protected static boolean placeBaseBlockIfPossible(LevelAccessor level, BlockPos pos, Block baseBlock, HolderSet<Block> replaceableBlocks) {
        if (!level.getBlockState(pos).is(replaceableBlocks)) {
            return false;
        }
        level.setBlock(pos, baseBlock.defaultBlockState(), 2);
        return true;
    }
    public static boolean isBaseOrLava(BlockState state, Block baseBlock, HolderSet<Block> replaceableBlocks) {
        return state.is(Blocks.LAVA) || isBase(state, baseBlock, replaceableBlocks);
    }
    public static boolean isBase(BlockState state, Block baseBlock, HolderSet<Block> replaceableBlocks) {
        return state.is(replaceableBlocks) || state.is(baseBlock);
    }
    public static boolean isEmptyOrWater(BlockState state) {
        return state.is(Blocks.WATER) || state.isAir();
    }
    public static boolean isNeitherEmptyNorWater(BlockState state) {
        return !isEmptyOrWater(state);
    }
    public static boolean isEmptyOrWaterOrLava(BlockState state) {
        return isEmptyOrWater(state) || state.is(Blocks.LAVA);
    }
}
