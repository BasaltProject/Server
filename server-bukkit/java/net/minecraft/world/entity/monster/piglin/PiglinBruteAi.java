// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.piglin;

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
import net.minecraft.util.Unit;
import net.minecraft.core.GlobalPos;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.sensing.Sensor;

public class PiglinBruteAi {
    private static final int ANGER_DURATION = 600;
    private static final int MELEE_ATTACK_COOLDOWN = 20;
    private static final double ACTIVITY_SOUND_LIKELIHOOD_PER_TICK = 0.0125;
    private static final int MAX_LOOK_DIST = 8;
    private static final int INTERACTION_RANGE = 8;
    private static final float SPEED_MULTIPLIER_WHEN_IDLING = 0.6F;
    private static final int HOME_CLOSE_ENOUGH_DISTANCE = 2;
    private static final int HOME_TOO_FAR_DISTANCE = 100;
    private static final int HOME_STROLL_AROUND_DISTANCE = 5;

    public static List<ActivityData<PiglinBrute>> getActivities(PiglinBrute piglin) {
        return List.of(coreActivity(), idleActivity(), fightActivity(piglin));
    }

    protected static void initMemories(PiglinBrute body) {
        body.getBrain().setMemory(MemoryModuleType.HOME, GlobalPos.of(body.level().dimension(), body.blockPosition()));
    }

    private static ActivityData<PiglinBrute> coreActivity() {
        return ActivityData.<PiglinBrute>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super PiglinBrute>>of(new LookAtTargetSink(45, 90), new MoveToTargetSink(),
            InteractWithDoor.create(), StopBeingAngryIfTargetDead.create()));
    }

    private static ActivityData<PiglinBrute> idleActivity() {
        return ActivityData.<PiglinBrute>create(Activity.IDLE, 10, ImmutableList.<BehaviorControl<? super PiglinBrute>>of(
            StartAttacking.<PiglinBrute>create(PiglinBruteAi::findNearestValidAttackTarget), idleLookBehaviors(), idleMovementBehaviors(),
            SetLookAndInteract.create(EntityTypes.PLAYER, 4)));
    }

    private static ActivityData<PiglinBrute> fightActivity(PiglinBrute body) {
        return ActivityData.<PiglinBrute>create(Activity.FIGHT, 10, ImmutableList.<BehaviorControl<? super PiglinBrute>>of(
            StopAttackingIfTargetInvalid.<PiglinBrute>create((level, target) -> !isNearestValidAttackTarget(level, body, target)),
            SetWalkTargetFromAttackTargetIfTargetOutOfReach.create(1.0F), MeleeAttack.<PiglinBrute>create(MELEE_ATTACK_COOLDOWN)), MemoryModuleType.ATTACK_TARGET);
    }

    private static RunOne<PiglinBrute> idleLookBehaviors() {
        return new RunOne<PiglinBrute>(ImmutableList.of(Pair.of(SetEntityLookTarget.create(EntityTypes.PLAYER, 8.0F), 1), Pair.of(SetEntityLookTarget.create(EntityTypes.PIGLIN, 8.0F), 1),
            Pair.of(SetEntityLookTarget.create(EntityTypes.PIGLIN_BRUTE, 8.0F), 1), Pair.of(SetEntityLookTarget.create(8.0F), 1), Pair.of(new DoNothing(30, 60), 1)));
    }

    private static RunOne<PiglinBrute> idleMovementBehaviors() {
        return new RunOne<PiglinBrute>(ImmutableList.of(Pair.of(RandomStroll.stroll(SPEED_MULTIPLIER_WHEN_IDLING), 2),
            Pair.of(InteractWith.of(EntityTypes.PIGLIN, INTERACTION_RANGE, MemoryModuleType.INTERACTION_TARGET, SPEED_MULTIPLIER_WHEN_IDLING, 2), 2),
            Pair.of(InteractWith.of(EntityTypes.PIGLIN_BRUTE, INTERACTION_RANGE, MemoryModuleType.INTERACTION_TARGET, SPEED_MULTIPLIER_WHEN_IDLING, 2), 2),
            Pair.of(StrollToPoi.create(MemoryModuleType.HOME, SPEED_MULTIPLIER_WHEN_IDLING, HOME_CLOSE_ENOUGH_DISTANCE, HOME_TOO_FAR_DISTANCE), 2),
            Pair.of(StrollAroundPoi.create(MemoryModuleType.HOME, SPEED_MULTIPLIER_WHEN_IDLING, HOME_STROLL_AROUND_DISTANCE), 2), Pair.of(new DoNothing(30, 60), 1)));
    }

    protected static void updateActivity(PiglinBrute body) {
        Brain<PiglinBrute> brain = body.getBrain();
        Activity oldActivity = brain.getActiveNonCoreActivity().orElse(null);
        brain.setActiveActivityToFirstValid(List.of(Activity.FIGHT, Activity.IDLE));
        if (oldActivity != brain.getActiveNonCoreActivity().orElse(null)) {
            playActivitySound(body);
        }
        body.setAggressive(brain.hasMemoryValue(MemoryModuleType.ATTACK_TARGET));
    }

    private static boolean isNearestValidAttackTarget(ServerLevel level, AbstractPiglin body, LivingEntity target) {
        return findNearestValidAttackTarget(level, body).filter(nearest -> nearest == target).isPresent();
    }

    private static Optional<? extends LivingEntity> findNearestValidAttackTarget(ServerLevel level, AbstractPiglin body) {
        Optional<LivingEntity> angryAt = BehaviorUtils.getLivingEntityFromUUIDMemory(body, MemoryModuleType.ANGRY_AT);
        if (angryAt.isPresent() && Sensor.isEntityAttackableIgnoringLineOfSight(level, body, angryAt.get())) {
            return angryAt;
        }
        Optional<? extends LivingEntity> player = body.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_ATTACKABLE_PLAYER);
        return player.isPresent() ? player : body.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_NEMESIS);
    }

    protected static void wasHurtBy(ServerLevel level, PiglinBrute body, LivingEntity attacker) {
        if (!(attacker instanceof AbstractPiglin)) {
            PiglinAi.maybeRetaliate(level, body, attacker);
        }
    }

    protected static void setAngerTarget(PiglinBrute body, LivingEntity target) {
        body.getBrain().eraseMemory(MemoryModuleType.CANT_REACH_WALK_TARGET_SINCE);
        body.getBrain().setMemoryWithExpiry(MemoryModuleType.ANGRY_AT, target.getUUID(), ANGER_DURATION);
    }

    protected static void maybePlayActivitySound(PiglinBrute body) {
        if (body.level().getRandom().nextFloat() < ACTIVITY_SOUND_LIKELIHOOD_PER_TICK) {
            playActivitySound(body);
        }
    }

    private static void playActivitySound(PiglinBrute body) {
        body.getBrain().getActiveNonCoreActivity().ifPresent(activity -> {
            if (activity == Activity.FIGHT) {
                body.playAngrySound();
            }
        });
    }
}
