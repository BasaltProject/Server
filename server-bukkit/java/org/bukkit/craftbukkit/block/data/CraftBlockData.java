package org.bukkit.craftbukkit.block.data;

import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.Unsupported;
public class CraftBlockData {
    protected CraftBlockData() {
    }
    public static CraftBlockData createData(BlockState state) {
        throw Unsupported.hit("org.bukkit.craftbukkit.block.data.CraftBlockData.createData");
    }
    public String getAsString() {
        throw Unsupported.hit("org.bukkit.craftbukkit.block.data.CraftBlockData.getAsString");
    }
    public BlockState getState() {
        throw Unsupported.hit("org.bukkit.craftbukkit.block.data.CraftBlockData.getState");
    }
}
