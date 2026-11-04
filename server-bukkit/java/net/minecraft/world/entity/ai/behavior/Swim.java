// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Map;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.tags.FluidTags;
import net.minecraft.tags.TagKey;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.material.Fluid;

public class Swim<T extends Mob> extends Behavior<T> {
    private final float chance;
    private final TagKey<Fluid> fluid;

    public Swim(float chance) {
        this(chance, FluidTags.WATER);
    }

    public Swim(float chance, TagKey<Fluid> fluid) {
        super(Map.of());
        this.chance = chance;
        this.fluid = fluid;
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Mob body) {
        return body.isInWater() && body.getFluidHeight(fluid) > body.getFluidJumpThreshold() || body.isInLava();
    }

    protected boolean canStillUse(ServerLevel level, Mob body, long timestamp) {
        return checkExtraStartConditions(level, body);
    }

    protected void tick(ServerLevel level, Mob body, long timestamp) {
        if (body.getRandom().nextFloat() < chance) {
            body.getJumpControl().jump();
        }
    }
}
