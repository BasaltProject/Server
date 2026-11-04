// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import java.util.Map;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.shapes.CollisionContext;

public class StandingAndWallBlockItem extends BlockItem {
    protected final Block wallBlock;
    private final Direction attachmentDirection;

    public StandingAndWallBlockItem(Block block, Block wallBlock, Direction attachmentDirection, Item.Properties properties) {
        super(block, properties);
        this.wallBlock = wallBlock;
        this.attachmentDirection = attachmentDirection;
    }

    protected boolean canPlace(LevelReader level, BlockState possibleState, BlockPos pos) {
        return possibleState.canSurvive(level, pos);
    }

    @Override
    protected BlockState getPlacementState(BlockPlaceContext context) {
        BlockState wallState = wallBlock.getStateForPlacement(context);
        LevelReader level = context.getLevel();
        BlockPos pos = context.getClickedPos();
        BlockState chosen = null;
        for (Direction direction : context.getNearestLookingDirections()) {
            if (direction == attachmentDirection.getOpposite()) {
                continue;
            }
            BlockState candidate = direction == attachmentDirection ? getBlock().getStateForPlacement(context) : wallState;
            if (candidate != null && canPlace(level, candidate, pos)) {
                chosen = candidate;
                break;
            }
        }
        return chosen != null && level.isUnobstructed(chosen, pos, CollisionContext.empty()) ? chosen : null;
    }

    @Override
    public void registerBlocks(Map<Block, Item> map, Item item) {
        super.registerBlocks(map, item);
        map.put(wallBlock, item);
    }
}
