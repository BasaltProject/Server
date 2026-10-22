package org.bukkit.craftbukkit.block;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelAccessor;
import org.basaltmc.bukkit.Unsupported;
public class CraftBlock {
    protected CraftBlock() {
    }
    public static CraftBlock at(LevelAccessor level, BlockPos position) {
        throw Unsupported.hit("org.bukkit.craftbukkit.block.CraftBlock.at");
    }
}
