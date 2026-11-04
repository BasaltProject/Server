// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.hoglin;

import com.google.common.collect.ImmutableList;
import com.mojang.datafixers.util.Pair;
import java.util.List;
import java.util.Set;
import java.util.Map;
import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.entity.*;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.ai.behavior.*;
import net.minecraft.world.entity.ai.behavior.declarative.BehaviorBuilder;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.core.BlockPos;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.util.TimeUtil;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.sensing.Sensor;

public class HoglinAi {
    public static final int REPELLENT_DETECTION_RANGE_HORIZONTAL = 8;
    public static final int REPELLENT_DETECTION_RANGE_VERTICAL = 4;
    private static final UniformInt RETREAT_DURATION = TimeUtil.rangeOfSeconds(5, 20);
    private static final int ATTACK_DURATION = 200;
    private static final int DESIRED_DISTANCE_FROM_PIGLIN_WHEN_IDLING = 8;
    private static final int DESIRED_DISTANCE_FROM_PIGLIN_WHEN_RETREATING = 15;
    private static final int ATTACK_INTERVAL = 40;
    private static final int BABY_ATTACK_INTERVAL = 15;
    private static final int REPELLENT_PACIFY_TIME = 200;
    private static final UniformInt ADULT_FOLLOW_RANGE = UniformInt.of(5, 16);
    private static final float SPEED_MULTIPLIER_WHEN_AVOIDING_REPELLENT = 1.0F;
    private static final float SPEED_MULTIPLIER_WHEN_RETREATING = 1.3F;
    private static final float SPEED_MULTIPLIER_WHEN_MAKING_LOVE = 0.6F;
    private static final float SPEED_MULTIPLIER_WHEN_IDLING = 0.4F;
    private static final float SPEED_MULTIPLIER_WHEN_FOLLOWING_ADULT = 0.6F;

    protected static List<ActivityData<Hoglin>> getActivities() {
        return List.of(coreActivity(), idleActivity(), fightActivity(), retreatActivity());
    }

    private static ActivityData<Hoglin> coreActivity() {
        return ActivityData.<Hoglin>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super Hoglin>>of(new LookAtTargetSink(45, 90), new MoveToTargetSink()));
    }

    private static ActivityData<Hoglin> idleActivity() {
        return ActivityData.<Hoglin>create(Activity.IDLE, 10, ImmutableList.<BehaviorControl<? super Hoglin>>of(
            BecomePassiveIfMemoryPresent.create(MemoryModuleType.NEAREST_REPELLENT, REPELLENT_PACIFY_TIME),
            new AnimalMakeLove(EntityTypes.HOGLIN, SPEED_MULTIPLIER_WHEN_MAKING_LOVE, 2),
            SetWalkTargetAwayFrom.pos(MemoryModuleType.NEAREST_REPELLENT, SPEED_MULTIPLIER_WHEN_AVOIDING_REPELLENT, REPELLENT_DETECTION_RANGE_HORIZONTAL, true),
            StartAttacking.<Hoglin>create(HoglinAi::findNearestValidAttackTarget),
            BehaviorBuilder.<Hoglin>triggerIf(Hoglin::isAdult, SetWalkTargetAwayFrom.entity(MemoryModuleType.NEAREST_VISIBLE_ADULT_PIGLIN, SPEED_MULTIPLIER_WHEN_IDLING,
                DESIRED_DISTANCE_FROM_PIGLIN_WHEN_IDLING, false)),
            SetEntityLookTargetSometimes.create(8.0F, UniformInt.of(30, 60)), BabyFollowAdult.create(ADULT_FOLLOW_RANGE, SPEED_MULTIPLIER_WHEN_FOLLOWING_ADULT),
            idleMovementBehaviors()));
    }

    private static ActivityData<Hoglin> fightActivity() {
        return ActivityData.<Hoglin>create(Activity.FIGHT, 10, ImmutableList.<BehaviorControl<? super Hoglin>>of(
            BecomePassiveIfMemoryPresent.create(MemoryModuleType.NEAREST_REPELLENT, REPELLENT_PACIFY_TIME),
            new AnimalMakeLove(EntityTypes.HOGLIN, SPEED_MULTIPLIER_WHEN_MAKING_LOVE, 2), SetWalkTargetFromAttackTargetIfTargetOutOfReach.create(1.0F),
            BehaviorBuilder.<Hoglin>triggerIf(Hoglin::isAdult, MeleeAttack.<Hoglin>create(ATTACK_INTERVAL)),
            BehaviorBuilder.<Hoglin>triggerIf(AgeableMob::isBaby, MeleeAttack.<Hoglin>create(BABY_ATTACK_INTERVAL)), StopAttackingIfTargetInvalid.<Hoglin>create(),
            EraseMemoryIf.<Hoglin>create(HoglinAi::isBreeding, MemoryModuleType.ATTACK_TARGET)), MemoryModuleType.ATTACK_TARGET);
    }

    private static ActivityData<Hoglin> retreatActivity() {
        return ActivityData.<Hoglin>create(Activity.AVOID, 10, ImmutableList.<BehaviorControl<? super Hoglin>>of(
            SetWalkTargetAwayFrom.entity(MemoryModuleType.AVOID_TARGET, SPEED_MULTIPLIER_WHEN_RETREATING, DESIRED_DISTANCE_FROM_PIGLIN_WHEN_RETREATING, false),
            idleMovementBehaviors(), SetEntityLookTargetSometimes.create(8.0F, UniformInt.of(30, 60)),
            EraseMemoryIf.<Hoglin>create(HoglinAi::wantsToStopFleeing, MemoryModuleType.AVOID_TARGET)), MemoryModuleType.AVOID_TARGET);
    }

    private static RunOne<Hoglin> idleMovementBehaviors() {
        return new RunOne<Hoglin>(ImmutableList.of(Pair.of(RandomStroll.stroll(SPEED_MULTIPLIER_WHEN_IDLING), 2),
            Pair.of(SetWalkTargetFromLookTarget.create(SPEED_MULTIPLIER_WHEN_IDLING, 3), 2), Pair.of(new DoNothing(30, 60), 1)));
    }

    protected static void updateActivity(Hoglin body) {
        Brain<Hoglin> mind = body.getBrain();
        Activity previous = mind.getActiveNonCoreActivity().orElse(null);
        mind.setActiveActivityToFirstValid(List.of(Activity.FIGHT, Activity.AVOID, Activity.IDLE));
        if (previous != mind.getActiveNonCoreActivity().orElse(null)) {
            getSoundForCurrentActivity(body).ifPresent(body::makeSound);
        }
        body.setAggressive(mind.hasMemoryValue(MemoryModuleType.ATTACK_TARGET));
    }

    protected static void onHitTarget(Hoglin attackerBody, LivingEntity target) {
        if (attackerBody.isBaby()) {
            return;
        }
        boolean outnumbered = target.is(EntityTypes.PIGLIN) && piglinsOutnumberHoglins(attackerBody);
        if (!outnumbered) {
            broadcastAttackTarget(attackerBody, target);
            return;
        }
        setAvoidTarget(attackerBody, target);
        broadcastRetreat(attackerBody, target);
    }

    private static void broadcastRetreat(Hoglin body, LivingEntity target) {
        getVisibleAdultHoglins(body).forEach(hoglin -> retreatFromNearestTarget(hoglin, target));
    }

    private static void retreatFromNearestTarget(Hoglin body, LivingEntity newAvoidTarget) {
        Brain<Hoglin> mind = body.getBrain();
        LivingEntity fear = BehaviorUtils.getNearestTarget(body, mind.getMemory(MemoryModuleType.AVOID_TARGET), newAvoidTarget);
        setAvoidTarget(body, BehaviorUtils.getNearestTarget(body, mind.getMemory(MemoryModuleType.ATTACK_TARGET), fear));
    }

    private static void setAvoidTarget(Hoglin body, LivingEntity avoidTarget) {
        Brain<Hoglin> mind = body.getBrain();
        mind.eraseMemory(MemoryModuleType.ATTACK_TARGET);
        mind.eraseMemory(MemoryModuleType.WALK_TARGET);
        int ticks = RETREAT_DURATION.sample(body.level().getRandom());
        mind.setMemoryWithExpiry(MemoryModuleType.AVOID_TARGET, avoidTarget, ticks);
    }

    private static Optional<? extends LivingEntity> findNearestValidAttackTarget(ServerLevel level, Hoglin body) {
        return !isPacified(body) && !isBreeding(body) ? body.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_ATTACKABLE_PLAYER) : Optional.empty();
    }

    static boolean isPosNearNearestRepellent(Hoglin body, BlockPos pos) {
        Optional<BlockPos> repellent = body.getBrain().getMemory(MemoryModuleType.NEAREST_REPELLENT);
        return repellent.isPresent() && repellent.get().closerThan(pos, 8.0);
    }

    private static boolean wantsToStopFleeing(Hoglin body) {
        return body.isAdult() && !piglinsOutnumberHoglins(body);
    }

    private static boolean piglinsOutnumberHoglins(Hoglin body) {
        if (body.isBaby()) {
            return false;
        }
        int piglins = body.getBrain().getMemory(MemoryModuleType.VISIBLE_ADULT_PIGLIN_COUNT).orElse(0);
        int hoglins = body.getBrain().getMemory(MemoryModuleType.VISIBLE_ADULT_HOGLIN_COUNT).orElse(0) + 1;
        return piglins > hoglins;
    }

    protected static void wasHurtBy(ServerLevel level, Hoglin body, LivingEntity attacker) {
        for (MemoryModuleType<?> calmed : List.of(MemoryModuleType.PACIFIED, MemoryModuleType.BREED_TARGET)) {
            body.getBrain().eraseMemory(calmed);
        }
        if (!body.isBaby()) {
            maybeRetaliate(level, body, attacker);
            return;
        }
        retreatFromNearestTarget(body, attacker);
    }

    private static void maybeRetaliate(ServerLevel level, Hoglin body, LivingEntity attacker) {
        boolean afraid = body.getBrain().isActive(Activity.AVOID) && attacker.is(EntityTypes.PIGLIN);
        boolean ignorable = attacker.is(EntityTypes.HOGLIN)
            || BehaviorUtils.isOtherTargetMuchFurtherAwayThanCurrentAttackTarget(body, attacker, 4.0);
        if (afraid || ignorable || !Sensor.isEntityAttackable(level, body, attacker)) {
            return;
        }
        setAttackTarget(body, attacker);
        broadcastAttackTarget(body, attacker);
    }

    private static void setAttackTarget(Hoglin body, LivingEntity target) {
        Brain<Hoglin> mind = body.getBrain();
        mind.eraseMemory(MemoryModuleType.BREED_TARGET);
        mind.eraseMemory(MemoryModuleType.CANT_REACH_WALK_TARGET_SINCE);
        mind.setMemoryWithExpiry(MemoryModuleType.ATTACK_TARGET, target, ATTACK_DURATION);
    }

    private static void broadcastAttackTarget(Hoglin body, LivingEntity target) {
        getVisibleAdultHoglins(body).forEach(hoglin -> setAttackTargetIfCloserThanCurrent(hoglin, target));
    }

    private static void setAttackTargetIfCloserThanCurrent(Hoglin body, LivingEntity newTarget) {
        if (!isPacified(body)) {
            setAttackTarget(body, BehaviorUtils.getNearestTarget(body, body.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET), newTarget));
        }
    }

    public static Optional<SoundEvent> getSoundForCurrentActivity(Hoglin body) {
        return body.getBrain().getActiveNonCoreActivity().map(activity -> getSoundForActivity(body, activity));
    }

    private static SoundEvent getSoundForActivity(Hoglin body, Activity activity) {
        if (activity == Activity.AVOID || body.isConverting()) {
            return SoundEvents.HOGLIN_RETREAT;
        }
        if (activity == Activity.FIGHT) {
            return SoundEvents.HOGLIN_ANGRY;
        }
        return body.getBrain().hasMemoryValue(MemoryModuleType.NEAREST_REPELLENT) ? SoundEvents.HOGLIN_RETREAT : SoundEvents.HOGLIN_AMBIENT;
    }

    private static List<Hoglin> getVisibleAdultHoglins(Hoglin body) {
        return body.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_ADULT_HOGLINS).orElse(ImmutableList.of());
    }

    private static boolean isBreeding(Hoglin body) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.BREED_TARGET);
    }

    protected static boolean isPacified(Hoglin body) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.PACIFIED);
    }
}
