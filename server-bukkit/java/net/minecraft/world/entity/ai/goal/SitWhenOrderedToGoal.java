// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.world.entity.TamableAnimal;

public class SitWhenOrderedToGoal extends Goal {
    private final TamableAnimal mob;

    public SitWhenOrderedToGoal(TamableAnimal mob) {
        this.mob = mob;
        setFlags(EnumSet.of(Goal.Flag.JUMP, Goal.Flag.MOVE));
    }

    @Override
    public boolean canContinueToUse() {
        return mob.isOrderedToSit();
    }

    @Override
    public boolean canUse() {
        boolean grounded = mob.isInWater() || !mob.onGround();
        if (!mob.isTame() || grounded) {
            return false;
        }
        net.minecraft.world.entity.LivingEntity owner = mob.getOwner();
        if (owner == null) {
            return true;
        }
        return (!(mob.distanceToSqr(owner) < 144.0) || owner.getLastHurtByMob() == null) && mob.isOrderedToSit();
    }

    @Override
    public void start() {
        mob.getNavigation().stop();
        mob.setInSittingPose(true);
    }

    @Override
    public void stop() {
        mob.setInSittingPose(false);
    }
}
