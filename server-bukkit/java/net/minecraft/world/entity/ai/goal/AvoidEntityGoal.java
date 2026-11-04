// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import java.util.function.Predicate;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.ai.util.LandRandomPos;
import net.minecraft.world.level.pathfinder.Path;
import net.minecraft.world.phys.Vec3;

public class AvoidEntityGoal<T extends LivingEntity> extends Goal {
    protected final PathfinderMob mob;
    private final double walkSpeedModifier;
    private final double sprintSpeedModifier;
    protected T toAvoid;
    protected final float maxDist;
    protected Path path;
    protected final PathNavigation pathNav;
    protected final Class<T> avoidClass;
    protected final Predicate<? super LivingEntity> avoidPredicate;
    protected final Predicate<? super LivingEntity> predicateOnAvoidEntity;
    private Vec3 escape;

    public AvoidEntityGoal(PathfinderMob mob, Class<T> avoidClass, float maxDist, double walkSpeedModifier, double sprintSpeedModifier) {
        this(mob, avoidClass, entity -> true, maxDist, walkSpeedModifier, sprintSpeedModifier, entity -> true);
    }

    public AvoidEntityGoal(PathfinderMob mob, Class<T> avoidClass, Predicate<LivingEntity> avoidPredicate, float maxDist,
        double walkSpeedModifier, double sprintSpeedModifier, Predicate<? super LivingEntity> predicateOnAvoidEntity) {
        this.mob = mob;
        this.avoidClass = avoidClass;
        this.avoidPredicate = avoidPredicate;
        this.maxDist = maxDist;
        this.walkSpeedModifier = walkSpeedModifier;
        this.sprintSpeedModifier = sprintSpeedModifier;
        this.predicateOnAvoidEntity = predicateOnAvoidEntity;
        this.pathNav = mob.getNavigation();
        setFlags(EnumSet.of(Goal.Flag.MOVE));
    }

    public AvoidEntityGoal(PathfinderMob mob, Class<T> avoidClass, float maxDist, double walkSpeedModifier, double sprintSpeedModifier,
        Predicate<? super LivingEntity> predicateOnAvoidEntity) {
        this(mob, avoidClass, entity -> true, maxDist, walkSpeedModifier, sprintSpeedModifier, predicateOnAvoidEntity);
    }

    @Override
    @SuppressWarnings("unchecked")
    public boolean canUse() {
        toAvoid = null;
        if (!(mob.live() instanceof org.bukkit.entity.Mob live)) {
            return false;
        }
        double best = Double.MAX_VALUE;
        for (org.bukkit.entity.Entity candidate : live.getNearbyEntities(maxDist, 3.0, maxDist)) {
            Entity handle = Entity.fromBukkit(candidate);
            if (avoidClass.isInstance(handle) && handle instanceof LivingEntity living && avoidPredicate.test(living)
                && predicateOnAvoidEntity.test(living) && mob.distanceToSqr(living) < best) {
                best = mob.distanceToSqr(living);
                toAvoid = (T) living;
            }
        }
        if (toAvoid == null) {
            return false;
        }
        escape = LandRandomPos.getPosAway(mob, 16, 7, toAvoid.position());
        return escape != null && toAvoid.distanceToSqr(escape.x, escape.y, escape.z) >= toAvoid.distanceToSqr(mob);
    }

    @Override
    public boolean canContinueToUse() {
        return !pathNav.isDone();
    }

    @Override
    public void start() {
        pathNav.moveTo(escape.x, escape.y, escape.z, walkSpeedModifier);
    }

    @Override
    public void stop() {
        toAvoid = null;
    }

    @Override
    public void tick() {
        pathNav.setSpeedModifier(toAvoid != null && mob.distanceToSqr(toAvoid) < 49.0 ? sprintSpeedModifier : walkSpeedModifier);
    }
}
