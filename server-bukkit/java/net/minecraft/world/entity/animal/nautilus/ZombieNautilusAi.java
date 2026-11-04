// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.nautilus;

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
import net.minecraft.sounds.SoundEvents;

public class ZombieNautilusAi {
    private static final float SPEED_MULTIPLIER_WHEN_IDLING_IN_WATER = 1.0F;
    private static final float SPEED_MULTIPLIER_WHEN_TEMPTED = 0.9F;
    private static final float SPEED_WHEN_ATTACKING = 0.5F;
    private static final float ATTACK_KNOCKBACK_FORCE = 2.0F;
    private static final int TIME_BETWEEN_ATTACKS = 80;
    private static final double MAX_CHARGE_DISTANCE = 12.0;
    private static final double MAX_TARGET_DETECTION_DISTANCE = 11.0;

    public static List<ActivityData<ZombieNautilus>> getActivities() {
        return List.of(coreActivity(), idleActivity(), fightActivity());
    }

    private static ActivityData<ZombieNautilus> coreActivity() {
        return ActivityData.<ZombieNautilus>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super ZombieNautilus>>of(new LookAtTargetSink(45, 90),
            new MoveToTargetSink(), new CountDownCooldownTicks(MemoryModuleType.TEMPTATION_COOLDOWN_TICKS), new CountDownCooldownTicks(MemoryModuleType.CHARGE_COOLDOWN_TICKS),
            new CountDownCooldownTicks(MemoryModuleType.ATTACK_TARGET_COOLDOWN)));
    }

    private static ActivityData<ZombieNautilus> idleActivity() {
        return ActivityData.<ZombieNautilus>create(Activity.IDLE, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super ZombieNautilus>>>of(
            Pair.of(1, new FollowTemptation(mob -> SPEED_MULTIPLIER_WHEN_TEMPTED, mob -> mob.isBaby() ? 2.5 : 3.5)),
            Pair.of(2, StartAttacking.<ZombieNautilus>create(NautilusAi::findNearestValidAttackTarget)),
            Pair.of(3, new GateBehavior<ZombieNautilus>(Map.of(MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT), Set.of(), GateBehavior.OrderPolicy.ORDERED,
                GateBehavior.RunningPolicy.TRY_ALL, ImmutableList.of(Pair.of(RandomStroll.swim(SPEED_MULTIPLIER_WHEN_IDLING_IN_WATER), 2),
                    Pair.of(SetWalkTargetFromLookTarget.create(SPEED_MULTIPLIER_WHEN_IDLING_IN_WATER, 3), 3))))));
    }

    private static ActivityData<ZombieNautilus> fightActivity() {
        return ActivityData.<ZombieNautilus>create(Activity.FIGHT, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super ZombieNautilus>>>of(
            Pair.of(0, new ChargeAttack(TIME_BETWEEN_ATTACKS, NautilusAi.ATTACK_TARGET_CONDITIONS, SPEED_WHEN_ATTACKING, ATTACK_KNOCKBACK_FORCE, MAX_CHARGE_DISTANCE,
                MAX_TARGET_DETECTION_DISTANCE, SoundEvents.ZOMBIE_NAUTILUS_DASH))),
            Set.of(Pair.of(MemoryModuleType.ATTACK_TARGET, MemoryStatus.VALUE_PRESENT), Pair.of(MemoryModuleType.TEMPTING_PLAYER, MemoryStatus.VALUE_ABSENT),
                Pair.of(MemoryModuleType.BREED_TARGET, MemoryStatus.VALUE_ABSENT), Pair.of(MemoryModuleType.CHARGE_COOLDOWN_TICKS, MemoryStatus.VALUE_ABSENT)));
    }

    public static void updateActivity(ZombieNautilus body) {
        body.getBrain().setActiveActivityToFirstValid(List.of(Activity.FIGHT, Activity.IDLE));
    }
}
