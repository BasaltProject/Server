package org.bukkit.craftbukkit.block;

import net.minecraft.world.level.block.entity.BlockEntity;
import org.basaltmc.bukkit.Unsupported;
public abstract class CraftBlockEntityState<T> extends CraftBlockState {
    protected CraftBlockEntityState() {
    }
    public BlockEntity getBlockEntity() {
        throw Unsupported.hit("org.bukkit.craftbukkit.block.CraftBlockEntityState.getBlockEntity");
    }
}
