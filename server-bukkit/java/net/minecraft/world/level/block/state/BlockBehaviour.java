package net.minecraft.world.level.block.state;

import net.minecraft.core.TypedInstance;
import net.minecraft.world.flag.FeatureElement;
public abstract class BlockBehaviour implements FeatureElement {
    protected BlockBehaviour() {
    }

    public abstract static class BlockStateBase extends StateHolder implements TypedInstance {
        protected BlockStateBase() {
        }
    }
}
