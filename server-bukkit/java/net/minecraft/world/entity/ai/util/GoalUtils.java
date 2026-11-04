// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.util;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.phys.Vec3;

public class GoalUtils {
    public static boolean hasGroundPathNavigation(Mob mob) {
        return mob.getNavigation().canNavigateGround();
    }

    public static boolean mobRestricted(PathfinderMob mob, double horizontalDist) {
        return mob.hasHome() && mob.getHomePosition().closerToCenterThan(mob.position(), mob.getHomeRadius() + horizontalDist + 1.0);
    }

    public static boolean isOutsideLimits(BlockPos pos, PathfinderMob mob) {
        return mob.level().isOutsideBuildHeight(pos.getY());
    }

    public static boolean isRestricted(boolean restrict, PathfinderMob mob, BlockPos pos) {
        return restrict && !mob.isWithinHome(pos);
    }

    public static boolean isRestricted(boolean restrict, PathfinderMob mob, Vec3 pos) {
        return restrict && !mob.isWithinHome(pos);
    }

    public static boolean isNotStable(PathNavigation navigation, BlockPos pos) {
        return !navigation.isStableDestination(pos);
    }

    public static boolean isWater(PathfinderMob mob, BlockPos pos) {
        return mob.level().getFluidState(pos).is(net.minecraft.tags.FluidTags.WATER);
    }

    public static boolean hasMalus(PathfinderMob mob, BlockPos pos) {
        return mob.level().getFluidState(pos).is(net.minecraft.tags.FluidTags.LAVA)
            || mob.level().getFluidState(pos.below()).is(net.minecraft.tags.FluidTags.LAVA);
    }

    public static boolean isSolid(PathfinderMob mob, BlockPos pos) {
        return mob.level().getBlockState(pos).isSolid();
    }
}
