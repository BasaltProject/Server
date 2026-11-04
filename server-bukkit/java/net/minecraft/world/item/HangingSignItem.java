// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.WallHangingSignBlock;
import net.minecraft.world.level.block.state.BlockState;

public class HangingSignItem extends StandingAndWallBlockItem {
    public HangingSignItem(Block hangingSign, Block wallHangingSign, Item.Properties properties) {
        super(hangingSign, wallHangingSign, Direction.UP, properties);
    }

    @Override
    protected boolean canPlace(LevelReader level, BlockState possibleState, BlockPos pos) {
        if (possibleState.getBlock() instanceof WallHangingSignBlock wall && !wall.canPlace(possibleState, level, pos)) {
            return false;
        }
        return super.canPlace(level, possibleState, pos);
    }
}
