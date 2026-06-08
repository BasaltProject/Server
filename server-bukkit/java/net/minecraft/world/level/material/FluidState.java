package net.minecraft.world.level.material;

import net.minecraft.core.TypedInstance;
import net.minecraft.tags.TagKey;
import net.minecraft.world.level.block.state.StateHolder;
import org.basaltmc.bukkit.Unsupported;
public class FluidState extends StateHolder implements TypedInstance {
    protected FluidState() {
    }
    public boolean is(TagKey tagKey) {
        throw Unsupported.hit("net.minecraft.world.level.material.FluidState.is");
    }
    public boolean is(Fluid fluid) {
        throw Unsupported.hit("net.minecraft.world.level.material.FluidState.is");
    }
}
