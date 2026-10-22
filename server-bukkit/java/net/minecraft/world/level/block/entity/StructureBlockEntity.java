package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.Unsupported;
public class StructureBlockEntity extends BlockEntity implements BoundingBoxRenderable {
    public StructureBlockEntity(BlockPos blockPos, BlockState blockState) {
    }
    public void setLevel(Level level) {
        throw Unsupported.hit("net.minecraft.world.level.block.entity.StructureBlockEntity.setLevel");
    }
}
