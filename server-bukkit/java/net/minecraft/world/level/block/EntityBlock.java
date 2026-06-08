package net.minecraft.world.level.block;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;

public interface EntityBlock {
    default BlockEntity newBlockEntity(BlockPos pos, BlockState state) {
        return null;
    }
}
