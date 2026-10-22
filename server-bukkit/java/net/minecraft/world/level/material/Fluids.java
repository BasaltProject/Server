package net.minecraft.world.level.material;

import net.minecraft.world.level.LevelReader;

public class Fluids {
    public static final Fluid EMPTY = new Fluid() {
        @Override
        public boolean isEmpty() {
            return true;
        }

        @Override
        public int getTickDelay(LevelReader level) {
            return 0;
        }

        @Override
        public String toString() {
            return "empty";
        }
    };
    public static final FlowingFluid FLOWING_WATER = new FlowingFluid("flowing_water", 5);
    public static final FlowingFluid WATER = new FlowingFluid("water", 5);
    public static final FlowingFluid FLOWING_LAVA = new FlowingFluid("flowing_lava", 30);
    public static final FlowingFluid LAVA = new FlowingFluid("lava", 30);

    private Fluids() {}
}
