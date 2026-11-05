// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
public enum EmptyBlockGetter implements BlockGetter {
    INSTANCE;
;
    public BlockEntity getBlockEntity(BlockPos pos) {
        return null;
    }
    public BlockState getBlockState(BlockPos pos) {
        return net.minecraft.world.level.block.Blocks.AIR.defaultBlockState();
    }
    public FluidState getFluidState(BlockPos pos) {
        return net.minecraft.world.level.material.Fluids.EMPTY.defaultFluidState();
    }
    public int getMinY() {
        return 0;
    }
    public int getHeight() {
        return 0;
    }
}
