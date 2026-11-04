// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.tags.TagKey;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.material.Fluid;

public class FloatGoal extends Goal {
    private final Mob mob;
    protected final TagKey<Fluid> fluid;

    public FloatGoal(Mob mob) {
        this(mob, null);
    }

    public FloatGoal(Mob mob, TagKey<Fluid> fluid) {
        this.mob = mob;
        this.fluid = fluid;
        setFlags(EnumSet.of(Goal.Flag.JUMP));
    }

    @Override
    public boolean canUse() {
        return mob.isInWater() || mob.isInLava();
    }

    @Override
    public boolean requiresUpdateEveryTick() {
        return true;
    }

    @Override
    public void tick() {
        if (mob.getRandom().nextFloat() < 0.8F) {
            mob.getJumpControl().jump();
        }
    }
}
