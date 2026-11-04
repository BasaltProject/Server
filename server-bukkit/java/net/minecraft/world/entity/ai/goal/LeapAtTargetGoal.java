// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.phys.Vec3;

public class LeapAtTargetGoal extends Goal {
    private final Mob mob;
    private final float yd;
    private LivingEntity target;

    public LeapAtTargetGoal(Mob mob, float yd) {
        this.mob = mob;
        this.yd = yd;
        setFlags(EnumSet.of(Goal.Flag.JUMP, Goal.Flag.MOVE));
    }

    @Override
    public boolean canUse() {
        if (mob.isVehicle()) {
            return false;
        }
        target = mob.getTarget();
        if (target == null) {
            return false;
        }
        double distance = mob.distanceToSqr(target);
        return distance >= 4.0 && distance <= 16.0 && mob.onGround() && mob.getRandom().nextInt(reducedTickDelay(5)) == 0;
    }

    @Override
    public boolean canContinueToUse() {
        return !mob.onGround();
    }

    @Override
    public void start() {
        Vec3 motion = mob.getDeltaMovement();
        Vec3 toTarget = new Vec3(target.getX() - mob.getX(), 0.0, target.getZ() - mob.getZ());
        if (toTarget.lengthSqr() > 1.0E-7) {
            toTarget = toTarget.normalize().scale(0.4).add(motion.scale(0.2));
        }
        mob.setDeltaMovement(toTarget.x, yd, toTarget.z);
    }
}
