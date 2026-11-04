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
import net.minecraft.tags.EntityTypeTags;
import net.minecraft.tags.ItemTags;
import net.minecraft.world.entity.ai.memory.NearestVisibleLivingEntities;
import net.minecraft.world.entity.ai.sensing.Sensor;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.gamerules.GameRules;

public class NautilusAi {
    private static final float SPEED_MULTIPLIER_WHEN_IDLING_IN_WATER = 1.0F;
    private static final float SPEED_MULTIPLIER_WHEN_TEMPTED = 1.3F;
    private static final float SPEED_MULTIPLIER_WHEN_MAKING_LOVE = 0.4F;
    private static final float SPEED_MULTIPLIER_WHEN_PANICKING = 1.6F;
    private static final UniformInt TIME_BETWEEN_NON_PLAYER_ATTACKS = UniformInt.of(2400, 3600);
    private static final float SPEED_WHEN_ATTACKING = 0.6F;
    private static final float ATTACK_KNOCKBACK_FORCE = 2.0F;
    private static final int ANGER_DURATION = 400;
    private static final int TIME_BETWEEN_ATTACKS = 80;
    private static final double MAX_CHARGE_DISTANCE = 12.0;
    private static final double MAX_TARGET_DETECTION_DISTANCE = 11.0;
    protected static final TargetingConditions ATTACK_TARGET_CONDITIONS = TargetingConditions.forCombat().selector(
        (target, level) -> (level.getGameRules().get(GameRules.MOB_GRIEFING) || !target.is(EntityTypes.ARMOR_STAND)) && level.getWorldBorder().isWithinBounds(target.getBoundingBox()));

    protected static void initMemories(AbstractNautilus body, RandomSource random) {
        body.getBrain().setMemory(MemoryModuleType.ATTACK_TARGET_COOLDOWN, TIME_BETWEEN_NON_PLAYER_ATTACKS.sample(random));
    }

    public static List<ActivityData<Nautilus>> getActivities() {
        return List.of(coreActivity(), idleActivity(), fightActivity());
    }

    private static ActivityData<Nautilus> coreActivity() {
        return ActivityData.<Nautilus>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super Nautilus>>of(new AnimalPanic<>(SPEED_MULTIPLIER_WHEN_PANICKING),
            new LookAtTargetSink(45, 90), new MoveToTargetSink(), new CountDownCooldownTicks(MemoryModuleType.TEMPTATION_COOLDOWN_TICKS),
            new CountDownCooldownTicks(MemoryModuleType.CHARGE_COOLDOWN_TICKS), new CountDownCooldownTicks(MemoryModuleType.ATTACK_TARGET_COOLDOWN)));
    }

    private static ActivityData<Nautilus> idleActivity() {
        return ActivityData.<Nautilus>create(Activity.IDLE, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Nautilus>>>of(
            Pair.of(1, new AnimalMakeLove(EntityTypes.NAUTILUS, SPEED_MULTIPLIER_WHEN_MAKING_LOVE, 2)),
            Pair.of(2, new FollowTemptation(mob -> SPEED_MULTIPLIER_WHEN_TEMPTED, mob -> mob.isBaby() ? 2.5 : 3.5)),
            Pair.of(3, StartAttacking.<Nautilus>create(NautilusAi::findNearestValidAttackTarget)),
            Pair.of(4, new GateBehavior<Nautilus>(Map.of(MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT), Set.of(), GateBehavior.OrderPolicy.ORDERED,
                GateBehavior.RunningPolicy.TRY_ALL, ImmutableList.of(Pair.of(RandomStroll.swim(SPEED_MULTIPLIER_WHEN_IDLING_IN_WATER), 2),
                    Pair.of(SetWalkTargetFromLookTarget.create(SPEED_MULTIPLIER_WHEN_IDLING_IN_WATER, 3), 3))))));
    }

    private static ActivityData<Nautilus> fightActivity() {
        return ActivityData.<Nautilus>create(Activity.FIGHT, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Nautilus>>>of(
            Pair.of(0, new ChargeAttack(TIME_BETWEEN_ATTACKS, ATTACK_TARGET_CONDITIONS, SPEED_WHEN_ATTACKING, ATTACK_KNOCKBACK_FORCE, MAX_CHARGE_DISTANCE,
                MAX_TARGET_DETECTION_DISTANCE, SoundEvents.NAUTILUS_DASH))),
            Set.of(Pair.of(MemoryModuleType.ATTACK_TARGET, MemoryStatus.VALUE_PRESENT), Pair.of(MemoryModuleType.TEMPTING_PLAYER, MemoryStatus.VALUE_ABSENT),
                Pair.of(MemoryModuleType.BREED_TARGET, MemoryStatus.VALUE_ABSENT), Pair.of(MemoryModuleType.CHARGE_COOLDOWN_TICKS, MemoryStatus.VALUE_ABSENT)));
    }

    protected static Optional<? extends LivingEntity> findNearestValidAttackTarget(ServerLevel level, AbstractNautilus body) {
        if (BehaviorUtils.isBreeding(body) || !body.isInWater() || body.isBaby() || body.isTame()) {
            return Optional.empty();
        }
        Optional<LivingEntity> angryAt = BehaviorUtils.getLivingEntityFromUUIDMemory(body, MemoryModuleType.ANGRY_AT)
            .filter(entity -> entity.isInWater() && Sensor.isEntityAttackableIgnoringLineOfSight(level, body, entity));
        if (angryAt.isPresent()) {
            return angryAt;
        }
        if (body.getBrain().hasMemoryValue(MemoryModuleType.ATTACK_TARGET_COOLDOWN)) {
            return Optional.empty();
        }
        RandomSource random = level.getRandom();
        body.getBrain().setMemory(MemoryModuleType.ATTACK_TARGET_COOLDOWN, TIME_BETWEEN_NON_PLAYER_ATTACKS.sample(random));
        if (random.nextFloat() < 0.5F) {
            return Optional.empty();
        }
        return body.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES).orElse(NearestVisibleLivingEntities.empty()).findClosest(NautilusAi::isHostileTarget);
    }

    protected static void setAngerTarget(ServerLevel level, AbstractNautilus body, LivingEntity target) {
        if (Sensor.isEntityAttackableIgnoringLineOfSight(level, body, target)) {
            body.getBrain().eraseMemory(MemoryModuleType.CANT_REACH_WALK_TARGET_SINCE);
            body.getBrain().setMemoryWithExpiry(MemoryModuleType.ANGRY_AT, target.getUUID(), ANGER_DURATION);
        }
    }

    private static boolean isHostileTarget(LivingEntity mob) {
        return mob.isInWater() && mob.is(EntityTypeTags.NAUTILUS_HOSTILES);
    }

    public static void updateActivity(Nautilus body) {
        body.getBrain().setActiveActivityToFirstValid(List.of(Activity.FIGHT, Activity.IDLE));
    }

    public static Predicate<ItemStack> getTemptations() {
        return item -> item.is(ItemTags.NAUTILUS_FOOD);
    }
}
