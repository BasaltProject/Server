package org.bukkit.craftbukkit.block;

import net.minecraft.core.BlockPos;
import org.basaltmc.bukkit.Unsupported;
import org.bukkit.block.data.BlockData;
public class CraftBlockState {
    protected CraftBlockState() {
    }
    public BlockData getBlockData() {
        throw Unsupported.hit("org.bukkit.craftbukkit.block.CraftBlockState.getBlockData");
    }
    public BlockPos getPosition() {
        throw Unsupported.hit("org.bukkit.craftbukkit.block.CraftBlockState.getPosition");
    }
}
