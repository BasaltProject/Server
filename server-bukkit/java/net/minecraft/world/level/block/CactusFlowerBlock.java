// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.Direction;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public class CactusFlowerBlock extends VegetationBlock {
    public CactusFlowerBlock(BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    protected boolean mayPlaceOn(BlockState state, BlockGetter level, BlockPos pos) {
        BlockState below = level.getBlockState(pos);
        return below.is(net.minecraft.tags.BlockTags.SUPPORT_OVERRIDE_CACTUS_FLOWER)
            || below.isFaceSturdy(level, pos, Direction.UP, net.minecraft.world.level.block.SupportType.CENTER);
    }
}
