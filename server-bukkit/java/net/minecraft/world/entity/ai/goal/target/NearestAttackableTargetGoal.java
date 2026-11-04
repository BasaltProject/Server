// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal.target;

import java.util.EnumSet;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.phys.AABB;

public class NearestAttackableTargetGoal<T extends LivingEntity> extends TargetGoal {
    protected final Class<T> targetType;
    protected final int randomInterval;
    protected LivingEntity target;
    protected final TargetingConditions targetConditions;

    public NearestAttackableTargetGoal(Mob mob, Class<T> targetType, boolean mustSee) {
        this(mob, targetType, 10, mustSee, false, null);
    }

    public NearestAttackableTargetGoal(Mob mob, Class<T> targetType, boolean mustSee, TargetingConditions.Selector selector) {
        this(mob, targetType, 10, mustSee, false, selector);
    }

    public NearestAttackableTargetGoal(Mob mob, Class<T> targetType, boolean mustSee, boolean mustReach) {
        this(mob, targetType, 10, mustSee, mustReach, null);
    }

    public NearestAttackableTargetGoal(Mob mob, Class<T> targetType, int randomInterval, boolean mustSee, boolean mustReach,
        TargetingConditions.Selector selector) {
        super(mob, mustSee, mustReach);
        this.targetType = targetType;
        this.randomInterval = reducedTickDelay(randomInterval);
        setFlags(EnumSet.of(Goal.Flag.TARGET));
        this.targetConditions = TargetingConditions.forCombat().range(getFollowDistance()).selector(selector);
    }

    @Override
    public boolean canUse() {
        if (randomInterval > 0 && mob.getRandom().nextInt(randomInterval) != 0) {
            return false;
        }
        findTarget();
        return target != null;
    }

    protected AABB getTargetSearchArea(double followDistance) {
        return mob.getBoundingBox().inflate(followDistance, 4.0, followDistance);
    }

    protected void findTarget() {
        double reach = getFollowDistance();
        target = null;
        double best = Double.MAX_VALUE;
        if (mob.live() instanceof org.bukkit.entity.Mob live) {
            for (org.bukkit.entity.Entity candidate : live.getNearbyEntities(reach, 4.0, reach)) {
                Entity handle = Entity.fromBukkit(candidate);
                if (targetType.isInstance(handle) && handle instanceof LivingEntity living
                    && canAttack(living, targetConditions)) {
                    double distance = mob.distanceToSqr(living);
                    if (distance < best) {
                        best = distance;
                        target = living;
                    }
                }
            }
        }
    }

    @Override
    public void start() {
        mob.setTarget(target);
        super.start();
    }

    public void setTarget(LivingEntity target) {
        this.target = target;
    }
}
