// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.util;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.phys.Vec3;

public class DefaultRandomPos {
    public static Vec3 getPos(PathfinderMob mob, int horizontalDist, int verticalDist) {
        boolean restrict = GoalUtils.mobRestricted(mob, horizontalDist);
        return RandomPos.generateRandomPos(mob, () -> {
            BlockPos direction = RandomPos.generateRandomDirection(mob.getRandom(), horizontalDist, verticalDist);
            return generateRandomPosTowardDirection(mob, horizontalDist, restrict, direction);
        });
    }

    public static Vec3 getPosTowards(PathfinderMob mob, int horizontalDist, int verticalDist, Vec3 towardsPos, double maxXzRadiansFromDir) {
        Vec3 dir = towardsPos.subtract(mob.getX(), mob.getY(), mob.getZ());
        boolean restrict = GoalUtils.mobRestricted(mob, horizontalDist);
        return RandomPos.generateRandomPos(mob, () -> {
            BlockPos direction = RandomPos.generateRandomDirectionWithinRadians(mob.getRandom(), 0.0, horizontalDist, verticalDist, 0, dir.x, dir.z,
                maxXzRadiansFromDir);
            return direction == null ? null : generateRandomPosTowardDirection(mob, horizontalDist, restrict, direction);
        });
    }

    public static Vec3 getPosAway(PathfinderMob mob, int horizontalDist, int verticalDist, Vec3 avoidPos) {
        Vec3 away = mob.position().subtract(avoidPos);
        boolean restrict = GoalUtils.mobRestricted(mob, horizontalDist);
        return RandomPos.generateRandomPos(mob, () -> {
            BlockPos direction = RandomPos.generateRandomDirectionWithinRadians(mob.getRandom(), 0.0, horizontalDist, verticalDist, 0, away.x, away.z,
                Math.PI / 2.0);
            return direction == null ? null : generateRandomPosTowardDirection(mob, horizontalDist, restrict, direction);
        });
    }

    private static BlockPos generateRandomPosTowardDirection(PathfinderMob mob, int horizontalDist, boolean restrict, BlockPos direction) {
        BlockPos pos = RandomPos.generateRandomPosTowardDirection(mob, horizontalDist, mob.getRandom(), direction);
        return !GoalUtils.isOutsideLimits(pos, mob) && !GoalUtils.isRestricted(restrict, mob, pos)
            && !GoalUtils.isNotStable(mob.getNavigation(), pos) && !GoalUtils.hasMalus(mob, pos) ? pos : null;
    }
}
