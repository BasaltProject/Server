// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.ScaffoldingBlock;
import net.minecraft.world.level.block.state.BlockState;

public class ScaffoldingBlockItem extends BlockItem {
    public ScaffoldingBlockItem(Block block, Item.Properties properties) {
        super(block, properties);
    }

    @Override
    public BlockPlaceContext updatePlacementContext(BlockPlaceContext context) {
        BlockPos clicked = context.getClickedPos();
        Level level = context.getLevel();
        if (!level.getBlockState(clicked).is(getBlock())) {
            return ScaffoldingBlock.getDistance(level, clicked) == 7 ? null : context;
        }
        Direction heading;
        if (context.isSecondaryUseActive()) {
            heading = context.isInside() ? context.getClickedFace().getOpposite() : context.getClickedFace();
        } else {
            heading = context.getClickedFace() == Direction.UP ? context.getHorizontalDirection() : Direction.UP;
        }
        int sideways = 0;
        BlockPos.MutableBlockPos cursor = clicked.mutable().move(heading);
        while (sideways < 7) {
            if (!level.isClientSide() && !level.isInWorldBounds(cursor)) {
                if (context.getPlayer() instanceof net.minecraft.server.level.ServerPlayer player && cursor.getY() > level.getMaxY()) {
                    player.sendBuildLimitMessage(true, level.getMaxY());
                }
                return null;
            }
            BlockState here = level.getBlockState(cursor);
            if (!here.is(getBlock())) {
                return here.canBeReplaced(context) ? BlockPlaceContext.at(context, cursor, heading) : null;
            }
            cursor.move(heading);
            if (heading.getAxis().isHorizontal()) {
                sideways++;
            }
        }
        return null;
    }

    @Override
    protected boolean mustSurvive() {
        return false;
    }
}
