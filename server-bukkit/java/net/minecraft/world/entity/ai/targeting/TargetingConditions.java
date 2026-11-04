// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.targeting;

import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.Difficulty;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;

public class TargetingConditions {
    public static final TargetingConditions DEFAULT = forCombat();
    private static final double kMinVisibilityDistanceForInvisibleTarget = 2.0;

    private final boolean isCombat;
    private double range = -1.0;
    private boolean checkLineOfSight = true;
    private boolean testInvisible = true;
    private Selector selector;

    private TargetingConditions(boolean isCombat) {
        this.isCombat = isCombat;
    }

    public static TargetingConditions forCombat() {
        return new TargetingConditions(true);
    }

    public static TargetingConditions forNonCombat() {
        return new TargetingConditions(false);
    }

    public TargetingConditions copy() {
        TargetingConditions clone = isCombat ? forCombat() : forNonCombat();
        clone.range = range;
        clone.checkLineOfSight = checkLineOfSight;
        clone.testInvisible = testInvisible;
        clone.selector = selector;
        return clone;
    }

    public TargetingConditions range(double range) {
        this.range = range;
        return this;
    }

    public TargetingConditions ignoreLineOfSight() {
        checkLineOfSight = false;
        return this;
    }

    public TargetingConditions ignoreInvisibilityTesting() {
        testInvisible = false;
        return this;
    }

    public TargetingConditions selector(Selector selector) {
        this.selector = selector;
        return this;
    }

    public boolean test(ServerLevel level, LivingEntity targeter, LivingEntity target) {
        if (targeter == target || !target.canBeSeenByAnyone() || (selector != null && !selector.test(target, level))) {
            return false;
        }
        if (targeter == null) {
            return !isCombat || (target.canBeSeenAsEnemy() && level.getDifficulty() != Difficulty.PEACEFUL);
        }
        if (isCombat && (!targeter.canAttack(target) || targeter.isAlliedTo(target))) {
            return false;
        }
        if (range > 0.0) {
            double modifier = testInvisible ? target.getVisibilityPercent(level, targeter) : 1.0;
            double visibilityDistance = Math.max(range * modifier, kMinVisibilityDistanceForInvisibleTarget);
            if (targeter.distanceToSqr(target.getX(), target.getY(), target.getZ()) > visibilityDistance * visibilityDistance) {
                return false;
            }
        }
        return !(checkLineOfSight && targeter instanceof Mob mob) || mob.getSensing().hasLineOfSight(target);
    }

    @FunctionalInterface
    public interface Selector {
        boolean test(LivingEntity target, ServerLevel level);
    }
}
