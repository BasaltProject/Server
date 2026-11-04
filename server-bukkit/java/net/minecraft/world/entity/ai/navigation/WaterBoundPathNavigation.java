// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.navigation;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.pathfinder.*;
import net.minecraft.world.phys.Vec3;

public class WaterBoundPathNavigation extends PathNavigation {
    private boolean allowBreaching;

    public WaterBoundPathNavigation(Mob mob, Level level) {
        super(mob, level);
    }

    @Override
    protected PathFinder createPathFinder(int maxVisitedNodes) {
        allowBreaching = mob.getType() == net.minecraft.world.entity.EntityTypes.DOLPHIN;
        nodeEvaluator = new SwimNodeEvaluator(allowBreaching);
        return new PathFinder(nodeEvaluator, maxVisitedNodes);
    }

    @Override
    protected boolean canUpdatePath() {
        return allowBreaching || mob.isInLiquid();
    }

    @Override
    public boolean isStableDestination(BlockPos pos) {
        return level.getFluidState(pos).is(net.minecraft.tags.FluidTags.WATER) || !level.getBlockState(pos).isAir() && allowBreaching;
    }

    @Override
    public boolean canNavigateGround() {
        return false;
    }
}
