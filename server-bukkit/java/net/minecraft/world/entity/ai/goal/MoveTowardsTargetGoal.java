// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.util.DefaultRandomPos;
import net.minecraft.world.phys.Vec3;

public class MoveTowardsTargetGoal extends Goal {
    private final PathfinderMob mob;
    private LivingEntity target;
    private double wantedX;
    private double wantedY;
    private double wantedZ;
    private final double speedModifier;
    private final float within;

    public MoveTowardsTargetGoal(PathfinderMob mob, double speedModifier, float within) {
        this.mob = mob;
        this.speedModifier = speedModifier;
        this.within = within;
        setFlags(EnumSet.of(Goal.Flag.MOVE));
    }

    @Override
    public boolean canUse() {
        target = mob.getTarget();
        if (target == null || target.distanceToSqr(mob) > (double) (within * within)) {
            return false;
        }
        Vec3 pos = DefaultRandomPos.getPosTowards(mob, 16, 7, target.position(), Math.PI / 2);
        if (pos == null) {
            return false;
        }
        wantedX = pos.x;
        wantedY = pos.y;
        wantedZ = pos.z;
        return true;
    }

    @Override
    public boolean canContinueToUse() {
        return !mob.getNavigation().isDone() && target.isAlive() && target.distanceToSqr(mob) < (double) (within * within);
    }

    @Override
    public void stop() {
        target = null;
    }

    @Override
    public void start() {
        mob.getNavigation().moveTo(wantedX, wantedY, wantedZ, speedModifier);
    }
}
