// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import net.minecraft.core.BlockPos;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;

public class DoubleHighBlockItem extends BlockItem {
    public DoubleHighBlockItem(Block block, Item.Properties properties) {
        super(block, properties);
    }

    @Override
    protected boolean placeBlock(BlockPlaceContext context, BlockState placementState) {
        Level level = context.getLevel();
        BlockPos upper = context.getClickedPos().above();
        level.setBlock(upper, level.isWaterAt(upper) ? Blocks.WATER.defaultBlockState() : Blocks.AIR.defaultBlockState(), 27);
        return super.placeBlock(context, placementState);
    }
}
