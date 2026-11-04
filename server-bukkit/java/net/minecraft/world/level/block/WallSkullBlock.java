// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import net.minecraft.core.Direction;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.block.state.properties.EnumProperty;

public class WallSkullBlock extends AbstractSkullBlock {
    public static final EnumProperty<Direction> FACING = BlockStateProperties.HORIZONTAL_FACING;

    protected WallSkullBlock(int index) {
        super(index);
    }

    public BlockState getStateForPlacement(BlockPlaceContext context) {
        for (Direction direction : context.getNearestLookingDirections()) {
            if (direction.getAxis().isHorizontal()) {
                BlockState candidate = defaultBlockState().setValue(FACING, direction.getOpposite());
                if (!context.getLevel().getBlockState(context.getClickedPos().relative(direction)).canBeReplaced(context)) {
                    return candidate;
                }
            }
        }
        return null;
    }
}
