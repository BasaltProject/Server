// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import net.minecraft.core.BlockPos;
import net.minecraft.tags.TagKey;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.level.material.Fluid;

public class TryFindLiquidGoal extends Goal {
    private final PathfinderMob mob;
    private final TagKey<Fluid> fluidTag;

    public TryFindLiquidGoal(PathfinderMob mob, TagKey<Fluid> fluidTag) {
        this.mob = mob;
        this.fluidTag = fluidTag;
    }

    @Override
    public boolean canUse() {
        return mob.onGround() && !mob.level().getFluidState(mob.blockPosition()).is(fluidTag);
    }

    @Override
    public void start() {
        for (BlockPos pos : BlockPos.betweenClosed(Mth.floor(mob.getX() - 2.0), Mth.floor(mob.getY() - 2.0),
                Mth.floor(mob.getZ() - 2.0), Mth.floor(mob.getX() + 2.0), mob.getBlockY(), Mth.floor(mob.getZ() + 2.0))) {
            if (mob.level().getFluidState(pos).is(fluidTag)) {
                mob.getMoveControl().setWantedPosition(pos.getX(), pos.getY(), pos.getZ(), 1.0);
                return;
            }
        }
    }
}
