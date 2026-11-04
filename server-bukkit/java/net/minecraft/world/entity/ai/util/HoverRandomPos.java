// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.util;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.phys.Vec3;

public class HoverRandomPos {
    public static Vec3 getPos(PathfinderMob mob, int horizontalDist, int verticalDist, double xDir, double zDir,
            float maxXzRadiansDifference, int hoverMaxHeight, int hoverMinHeight) {
        boolean restrict = GoalUtils.mobRestricted(mob, horizontalDist);
        return RandomPos.generateRandomPos(mob, () -> {
            BlockPos direction = RandomPos.generateRandomDirectionWithinRadians(mob.getRandom(), 0.0, horizontalDist, verticalDist, 0,
                    xDir, zDir, maxXzRadiansDifference);
            if (direction == null) {
                return null;
            }
            BlockPos pos = LandRandomPos.generateRandomPosTowardDirection(mob, horizontalDist, restrict, direction);
            if (pos == null) {
                return null;
            }
            pos = RandomPos.moveUpToAboveSolid(pos, mob.getRandom().nextInt(hoverMaxHeight - hoverMinHeight + 1) + hoverMinHeight,
                    mob.level().getMaxY(), p -> GoalUtils.isSolid(mob, p));
            return !GoalUtils.isWater(mob, pos) && !GoalUtils.hasMalus(mob, pos) ? pos : null;
        });
    }
}
