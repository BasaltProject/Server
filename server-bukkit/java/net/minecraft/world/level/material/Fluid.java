package net.minecraft.world.level.material;

import net.minecraft.world.level.LevelReader;

public abstract class Fluid {
    private final FluidState defaultState = new FluidState(this, true);

    public FluidState defaultFluidState() {
        return defaultState;
    }

    public boolean isEmpty() {
        return false;
    }

    public int getTickDelay(LevelReader level) {
        return 0;
    }
}
