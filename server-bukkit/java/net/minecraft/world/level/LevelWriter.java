// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.block.state.BlockState;
public interface LevelWriter {
    boolean setBlock(BlockPos pos, BlockState blockState, int updateFlags, int updateLimit);
    default boolean setBlockAndUpdate(BlockPos pos, BlockState blockState) {
        return setBlock(pos, blockState, 3);
    }
    default boolean setBlock(BlockPos pos, BlockState blockState, int updateFlags) {
        return setBlock(pos, blockState, updateFlags, 512);
    }
    default boolean removeBlock(BlockPos pos, boolean movedByPiston) {
        return setBlock(pos, net.minecraft.world.level.block.Blocks.AIR.defaultBlockState(), 3 | (movedByPiston ? 64 : 0));
    }
    default boolean destroyBlock(BlockPos pos, boolean dropResources) {
        return destroyBlock(pos, dropResources, null);
    }
    default boolean destroyBlock(BlockPos pos, boolean dropResources, Entity breaker) {
        return destroyBlock(pos, dropResources, breaker, 512);
    }
    boolean destroyBlock(BlockPos pos, boolean dropResources, Entity breaker, int updateLimit);
    default boolean addFreshEntity(Entity entity) {
        return false;
    }
}
