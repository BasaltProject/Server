package net.minecraft.world.level.material;

import net.minecraft.core.TypedInstance;
import net.minecraft.tags.FluidTags;
import net.minecraft.tags.TagKey;
import net.minecraft.world.level.block.state.StateHolder;

public class FluidState extends StateHolder<Fluid, FluidState> implements TypedInstance<Fluid> {
    private final boolean source;

    public FluidState(Fluid fluid, boolean source) {
        super(fluid);
        this.source = source;
    }

    public Fluid getType() {
        return owner;
    }

    public boolean isEmpty() {
        return owner.isEmpty();
    }

    public boolean isSource() {
        return source && !owner.isEmpty();
    }

    public boolean is(Fluid fluid) {
        return owner == fluid || fluid == Fluids.WATER && owner == Fluids.FLOWING_WATER
            || fluid == Fluids.LAVA && owner == Fluids.FLOWING_LAVA;
    }

    public boolean is(TagKey<Fluid> tag) {
        if (tag == FluidTags.WATER) {
            return is(Fluids.WATER);
        }
        return tag == FluidTags.LAVA && is(Fluids.LAVA);
    }
}
