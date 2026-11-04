// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;

public class OcelotAttackGoal extends Goal {
    private final Mob mob;
    private LivingEntity target;
    private int attackTime;

    public OcelotAttackGoal(Mob mob) {
        this.mob = mob;
        setFlags(EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
    }

    @Override
    public boolean canUse() {
        LivingEntity candidate = mob.getTarget();
        if (candidate == null) {
            return false;
        }
        target = candidate;
        return true;
    }

    @Override
    public boolean canContinueToUse() {
        if (!target.isAlive() || mob.distanceToSqr(target) > 225.0) {
            return false;
        }
        return !mob.getNavigation().isDone() || canUse();
    }

    @Override
    public void stop() {
        target = null;
        mob.getNavigation().stop();
    }

    @Override
    public boolean requiresUpdateEveryTick() {
        return true;
    }

    @Override
    public void tick() {
        mob.getLookControl().setLookAt(target, 30.0F, 30.0F);
        double reach = mob.getBbWidth() * 2.0F * mob.getBbWidth() * 2.0F;
        double distance = mob.distanceToSqr(target.getX(), target.getY(), target.getZ());
        double speed = 0.8;
        if (distance > reach && distance < 16.0) {
            speed = 1.33;
        } else if (distance < 225.0) {
            speed = 0.6;
        }
        mob.getNavigation().moveTo(target, speed);
        attackTime = Math.max(attackTime - 1, 0);
        if (distance <= reach && attackTime <= 0) {
            attackTime = 20;
            mob.doHurtTarget((net.minecraft.server.level.ServerLevel) mob.level(), target);
        }
    }
}
