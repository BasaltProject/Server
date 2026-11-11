// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import java.util.Map;
import net.minecraft.world.phys.shapes.Shapes;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import java.util.function.Function;
import net.minecraft.core.Direction;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.EnumProperty;
import net.minecraft.world.level.block.state.properties.IntegerProperty;
import net.minecraft.world.phys.shapes.VoxelShape;
public interface SegmentableBlock {
    int MIN_SEGMENT = 1;
    int MAX_SEGMENT = 4;
    IntegerProperty AMOUNT = net.minecraft.world.level.block.state.properties.BlockStateProperties.SEGMENT_AMOUNT;
    default Function<BlockState, VoxelShape> getShapeCalculator(EnumProperty<Direction> facing, IntegerProperty amount) {
        Map<Direction, VoxelShape> corner = Shapes.rotateHorizontal(Block.box(0.0, 0.0, 0.0, 8.0, getShapeHeight(), 8.0));
        return state -> {
            VoxelShape merged = Shapes.empty();
            Direction at = state.getValue(facing);
            for (int n = state.getValue(amount); n > 0; n--) {
                merged = Shapes.or(merged, corner.get(at));
                at = at.getCounterClockWise();
            }
            return merged.singleEncompassing();
        };
    }
    default IntegerProperty getSegmentAmountProperty() {
        return AMOUNT;
    }
    default double getShapeHeight() {
        return 1.0;
    }
    default boolean canBeReplaced(BlockState state, BlockPlaceContext context, IntegerProperty segment) {
        return !context.isSecondaryUseActive() && context.getItemInHand().is(state.getBlock().asItem()) && state.getValue(segment) < MAX_SEGMENT;
    }
    default BlockState getStateForPlacement(BlockPlaceContext context, Block block, IntegerProperty segment, EnumProperty<Direction> facing) {
        BlockState here = context.getLevel().getBlockState(context.getClickedPos());
        if (here.is(block)) {
            return here.setValue(segment, Math.min(MAX_SEGMENT, here.getValue(segment) + 1));
        }
        return block.defaultBlockState().setValue(facing, context.getHorizontalDirection().getOpposite());


    }
}
