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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.EmptyBlockGetter#getBlockEntity");
    }
    public BlockState getBlockState(BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.EmptyBlockGetter#getBlockState");
    }
    public FluidState getFluidState(BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.EmptyBlockGetter#getFluidState");
    }
    public int getMinY() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.EmptyBlockGetter#getMinY");
    }
    public int getHeight() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.EmptyBlockGetter#getHeight");
    }
}
