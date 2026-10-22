package net.minecraft.world.level.block;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.Unsupported;
public interface EntityBlock {
    public default BlockEntity newBlockEntity(BlockPos worldPosition, BlockState blockState) {
        throw Unsupported.hit("net.minecraft.world.level.block.EntityBlock.newBlockEntity");
    }
}
