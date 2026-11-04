// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.util;

import java.util.function.ToDoubleFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.phys.Vec3;

public class LandRandomPos {
    public static Vec3 getPos(PathfinderMob mob, int horizontalDist, int verticalDist) {
        return getPos(mob, horizontalDist, verticalDist, mob::getWalkTargetValue);
    }

    public static Vec3 getPos(PathfinderMob mob, int horizontalDist, int verticalDist, ToDoubleFunction<BlockPos> positionWeight) {
        boolean restrict = GoalUtils.mobRestricted(mob, horizontalDist);
        return RandomPos.generateRandomPos(() -> {
            BlockPos direction = RandomPos.generateRandomDirection(mob.getRandom(), horizontalDist, verticalDist);
            BlockPos pos = generateRandomPosTowardDirection(mob, horizontalDist, restrict, direction);
            return pos == null ? null : movePosUpOutOfSolid(mob, pos);
        }, positionWeight);
    }

    public static Vec3 getPosTowards(PathfinderMob mob, int horizontalDist, int verticalDist, Vec3 towardsPos) {
        Vec3 dir = towardsPos.subtract(mob.getX(), mob.getY(), mob.getZ());
        return getPosInDirection(mob, 0.0, horizontalDist, verticalDist, dir, GoalUtils.mobRestricted(mob, horizontalDist));
    }

    public static Vec3 getPosAway(PathfinderMob mob, int horizontalDist, int verticalDist, Vec3 avoidPos) {
        return getPosAway(mob, 0.0, horizontalDist, verticalDist, avoidPos);
    }

    public static Vec3 getPosAway(PathfinderMob mob, double minHorizontalDist, double maxHorizontalDist, int verticalDist, Vec3 avoidPos) {
        Vec3 away = mob.position().subtract(avoidPos);
        if (away.length() == 0.0) {
            away = new Vec3(mob.getRandom().nextDouble() - 0.5, 0.0, mob.getRandom().nextDouble() - 0.5);
        }
        return getPosInDirection(mob, minHorizontalDist, maxHorizontalDist, verticalDist, away, GoalUtils.mobRestricted(mob, maxHorizontalDist));
    }

    private static Vec3 getPosInDirection(PathfinderMob mob, double minHorizontalDist, double maxHorizontalDist, int verticalDist, Vec3 dir,
        boolean restrict) {
        return RandomPos.generateRandomPos(mob, () -> {
            BlockPos direction = RandomPos.generateRandomDirectionWithinRadians(mob.getRandom(), minHorizontalDist, maxHorizontalDist, verticalDist, 0,
                dir.x, dir.z, Math.PI / 2.0);
            if (direction == null) {
                return null;
            }
            BlockPos pos = generateRandomPosTowardDirection(mob, maxHorizontalDist, restrict, direction);
            return pos == null ? null : movePosUpOutOfSolid(mob, pos);
        });
    }

    public static BlockPos movePosUpOutOfSolid(PathfinderMob mob, BlockPos pos) {
        BlockPos moved = RandomPos.moveUpOutOfSolid(pos, mob.level().getMaxY(), blockPos -> GoalUtils.isSolid(mob, blockPos));
        return !GoalUtils.isWater(mob, moved) && !GoalUtils.hasMalus(mob, moved) ? moved : null;
    }

    public static BlockPos generateRandomPosTowardDirection(PathfinderMob mob, double horizontalDist, boolean restrict, BlockPos direction) {
        BlockPos pos = RandomPos.generateRandomPosTowardDirection(mob, horizontalDist, mob.getRandom(), direction);
        return !GoalUtils.isOutsideLimits(pos, mob) && !GoalUtils.isRestricted(restrict, mob, pos)
            && !GoalUtils.isNotStable(mob.getNavigation(), pos) ? pos : null;
    }
}
