package net.minecraft.world.level.material;

import net.minecraft.world.level.LevelReader;
import org.basaltmc.bukkit.Unsupported;
public abstract class Fluid {
    protected Fluid() {
    }
    public FluidState defaultFluidState() {
        throw Unsupported.hit("net.minecraft.world.level.material.Fluid.defaultFluidState");
    }
    public int getTickDelay(LevelReader level) {
        throw Unsupported.hit("net.minecraft.world.level.material.Fluid.getTickDelay");
    }
}
