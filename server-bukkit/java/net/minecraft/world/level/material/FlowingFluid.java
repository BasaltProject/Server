package net.minecraft.world.level.material;

import net.minecraft.world.level.LevelReader;

public class FlowingFluid extends Fluid {
    private final String name;
    private final int tickDelay;

    public FlowingFluid(String name, int tickDelay) {
        this.name = name;
        this.tickDelay = tickDelay;
    }

    @Override
    public int getTickDelay(LevelReader level) {
        return tickDelay;
    }

    @Override
    public String toString() {
        return name;
    }
}
