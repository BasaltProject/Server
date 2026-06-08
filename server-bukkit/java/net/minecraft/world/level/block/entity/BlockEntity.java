package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.core.TypedInstance;
import net.minecraft.util.debug.DebugValueSource;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import org.basaltmc.bukkit.Unsupported;
public abstract class BlockEntity implements DebugValueSource, TypedInstance {
    protected boolean remove;
    protected BlockEntity() {
    }
    public BlockPos getBlockPos() {
        throw Unsupported.hit("net.minecraft.world.level.block.entity.BlockEntity.getBlockPos");
    }
    public BlockState getBlockState() {
        throw Unsupported.hit("net.minecraft.world.level.block.entity.BlockEntity.getBlockState");
    }
    public Level getLevel() {
        throw Unsupported.hit("net.minecraft.world.level.block.entity.BlockEntity.getLevel");
    }
    public BlockEntityType getType() {
        throw Unsupported.hit("net.minecraft.world.level.block.entity.BlockEntity.getType");
    }
    public boolean isRemoved() {
        throw Unsupported.hit("net.minecraft.world.level.block.entity.BlockEntity.isRemoved");
    }
    public void loadWithComponents(ValueInput input) {
        throw Unsupported.hit("net.minecraft.world.level.block.entity.BlockEntity.loadWithComponents");
    }
    public void saveWithId(ValueOutput output) {
        throw Unsupported.hit("net.minecraft.world.level.block.entity.BlockEntity.saveWithId");
    }
    public void setBlockState(BlockState blockState) {
        throw Unsupported.hit("net.minecraft.world.level.block.entity.BlockEntity.setBlockState");
    }
    public void setChanged() {
        throw Unsupported.hit("net.minecraft.world.level.block.entity.BlockEntity.setChanged");
    }
}
