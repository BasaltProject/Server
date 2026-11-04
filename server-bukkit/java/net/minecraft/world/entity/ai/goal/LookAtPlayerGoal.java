// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;

public class LookAtPlayerGoal extends Goal {
    public static final float DEFAULT_PROBABILITY = 0.02F;
    protected final Mob mob;
    protected Entity lookAt;
    protected final float lookDistance;
    protected final float probability;
    private final boolean onlyHorizontal;
    protected final Class<? extends LivingEntity> lookAtType;
    protected final TargetingConditions lookAtContext = null;
    private int lookTime;

    public LookAtPlayerGoal(Mob mob, Class<? extends LivingEntity> lookAtType, float lookDistance) {
        this(mob, lookAtType, lookDistance, DEFAULT_PROBABILITY);
    }

    public LookAtPlayerGoal(Mob mob, Class<? extends LivingEntity> lookAtType, float lookDistance, float probability) {
        this(mob, lookAtType, lookDistance, probability, false);
    }

    public LookAtPlayerGoal(Mob mob, Class<? extends LivingEntity> lookAtType, float lookDistance, float probability, boolean onlyHorizontal) {
        this.mob = mob;
        this.lookAtType = lookAtType;
        this.lookDistance = lookDistance;
        this.probability = probability;
        this.onlyHorizontal = onlyHorizontal;
        setFlags(EnumSet.of(Goal.Flag.LOOK));
    }

    @Override
    public boolean canUse() {
        if (mob.getRandom().nextFloat() >= probability) {
            return false;
        }
        if (mob.getTarget() != null) {
            lookAt = mob.getTarget();
        } else if (mob.live() instanceof org.bukkit.entity.Mob live) {
            double reach = lookDistance;
            org.bukkit.entity.Entity best = null;
            double bestDistance = Double.MAX_VALUE;
            for (org.bukkit.entity.Entity candidate : live.getNearbyEntities(reach, reach, reach)) {
                Entity handle = Entity.fromBukkit(candidate);
                double distance = candidate.getLocation().distanceSquared(live.getLocation());
                if (lookAtType.isInstance(handle) && distance < bestDistance) {
                    best = candidate;
                    bestDistance = distance;
                }
            }
            lookAt = best == null ? null : Entity.fromBukkit(best);
        }
        return lookAt != null;
    }

    @Override
    public boolean canContinueToUse() {
        return lookAt != null && lookAt.isAlive() && mob.distanceToSqr(lookAt) <= lookDistance * lookDistance && lookTime > 0;
    }

    @Override
    public void start() {
        lookTime = adjustedTickDelay(40 + mob.getRandom().nextInt(40));
    }

    @Override
    public void stop() {
        lookAt = null;
    }

    @Override
    public void tick() {
        if (lookAt != null && lookAt.isAlive()) {
            mob.getLookControl().setLookAt(lookAt.getX(), onlyHorizontal ? mob.getEyeY() : lookAt.getEyeY(), lookAt.getZ());
            lookTime--;
        }
    }
}
