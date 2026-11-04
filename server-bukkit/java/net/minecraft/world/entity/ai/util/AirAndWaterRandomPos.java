// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.util;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.phys.Vec3;

public class AirAndWaterRandomPos {
    public static Vec3 getPos(PathfinderMob mob, int horizontalDist, int verticalDist, int flyingHeight, double xDir, double zDir,
            double maxXzRadiansDifference) {
        boolean restrict = GoalUtils.mobRestricted(mob, horizontalDist);
        return RandomPos.generateRandomPos(mob, () -> generateRandomPos(mob, horizontalDist, verticalDist, flyingHeight, xDir, zDir,
                maxXzRadiansDifference, restrict));
    }

    public static BlockPos generateRandomPos(PathfinderMob mob, int horizontalDist, int verticalDist, int flyingHeight, double xDir,
            double zDir, double maxXzRadiansDifference, boolean restrict) {
        BlockPos direction = RandomPos.generateRandomDirectionWithinRadians(mob.getRandom(), 0.0, horizontalDist, verticalDist,
                flyingHeight, xDir, zDir, maxXzRadiansDifference);
        if (direction == null) {
            return null;
        }
        BlockPos pos = RandomPos.generateRandomPosTowardDirection(mob, horizontalDist, mob.getRandom(), direction);
        if (GoalUtils.isOutsideLimits(pos, mob) || GoalUtils.isRestricted(restrict, mob, pos)) {
            return null;
        }
        pos = RandomPos.moveUpOutOfSolid(pos, mob.level().getMaxY(), p -> GoalUtils.isSolid(mob, p));
        return GoalUtils.hasMalus(mob, pos) ? null : pos;
    }
}
