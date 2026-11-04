// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.MoverType;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.phys.Vec3;

public class FollowPlayerRiddenEntityGoal extends Goal {
    private int timeToRecalcPath;
    private final PathfinderMob mob;
    private final Class<? extends Entity> entityTypeToFollow;
    private Player following;
    private Phase phase;

    public FollowPlayerRiddenEntityGoal(PathfinderMob mob, Class<? extends Entity> entityTypeToFollow) {
        this.mob = mob;
        this.entityTypeToFollow = entityTypeToFollow;
    }

    private Player findRider() {
        for (Entity candidate : mob.level().getEntitiesOfClass(entityTypeToFollow, mob.getBoundingBox().inflate(5.0))) {
            if (candidate.getControllingPassenger() instanceof Player rider) {
                return rider;
            }
        }
        return null;
    }

    public boolean canUse() {
        if (following != null && following.hasMovedHorizontallyRecently()) {
            return true;
        }
        Player rider = findRider();
        return rider != null && rider.hasMovedHorizontallyRecently();
    }

    public boolean isInterruptable() {
        return true;
    }

    public boolean canContinueToUse() {
        return following != null && following.isPassenger() && following.hasMovedHorizontallyRecently();
    }

    public void start() {
        following = findRider();
        timeToRecalcPath = 0;
        phase = Phase.GO_TO_ENTITY;
    }

    public void stop() {
        following = null;
    }

    public void tick() {
        float speed = phase == Phase.GO_IN_ENTITY_DIRECTION ? 0.01F : 0.015F;
        mob.moveRelative(speed, new Vec3(mob.xxa, mob.yya, mob.zza));
        mob.move(MoverType.SELF, mob.getDeltaMovement());
        if (--timeToRecalcPath > 0 || following == null) {
            return;
        }
        timeToRecalcPath = adjustedTickDelay(10);
        if (phase == Phase.GO_TO_ENTITY) {
            BlockPos behind = following.blockPosition().relative(following.getDirection().getOpposite()).offset(0, -1, 0);
            mob.getNavigation().moveTo(behind.getX(), behind.getY(), behind.getZ(), 1.0);
            if (mob.distanceTo(following) < 4.0F) {
                timeToRecalcPath = 0;
                phase = Phase.GO_IN_ENTITY_DIRECTION;
            }
        } else {
            Direction heading = following.getMotionDirection();
            BlockPos ahead = following.blockPosition().relative(heading, 10);
            mob.getNavigation().moveTo(ahead.getX(), ahead.getY() - 1, ahead.getZ(), 1.0);
            if (mob.distanceTo(following) > 12.0F) {
                timeToRecalcPath = 0;
                phase = Phase.GO_TO_ENTITY;
            }
        }
    }

    private enum Phase {
        GO_TO_ENTITY,
        GO_IN_ENTITY_DIRECTION
    }
}
