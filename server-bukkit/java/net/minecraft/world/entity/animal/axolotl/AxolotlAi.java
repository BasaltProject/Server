// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.axolotl;

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
import net.minecraft.tags.FluidTags;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.behavior.declarative.BehaviorBuilder;
import net.minecraft.world.entity.ai.behavior.PositionTracker;

public class AxolotlAi {
    private static final UniformInt ADULT_FOLLOW_RANGE = UniformInt.of(5, 16);
    private static final float SPEED_MULTIPLIER_WHEN_MAKING_LOVE = 0.2F;
    private static final float SPEED_MULTIPLIER_ON_LAND = 0.15F;
    private static final float SPEED_MULTIPLIER_WHEN_IDLING_IN_WATER = 0.5F;
    private static final float SPEED_MULTIPLIER_WHEN_CHASING_IN_WATER = 0.6F;
    private static final float SPEED_MULTIPLIER_WHEN_FOLLOWING_ADULT_IN_WATER = 0.6F;

    protected static List<ActivityData<Axolotl>> getActivities() {
        return List.of(initCoreActivity(), initIdleActivity(), initFightActivity(), initPlayDeadActivity());
    }

    protected static ActivityData<Axolotl> initPlayDeadActivity() {
        return ActivityData.<Axolotl>create(Activity.PLAY_DEAD, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Axolotl>>>of(
            Pair.of(0, new PlayDead()), Pair.of(1, EraseMemoryIf.<Axolotl>create(BehaviorUtils::isBreeding, MemoryModuleType.PLAY_DEAD_TICKS))),
            Set.of(Pair.of(MemoryModuleType.PLAY_DEAD_TICKS, MemoryStatus.VALUE_PRESENT)), Set.of(MemoryModuleType.PLAY_DEAD_TICKS));
    }

    protected static ActivityData<Axolotl> initFightActivity() {
        return ActivityData.<Axolotl>create(Activity.FIGHT, 0, ImmutableList.<BehaviorControl<? super Axolotl>>of(
            StopAttackingIfTargetInvalid.<Axolotl>create(Axolotl::onStopAttacking),
            SetWalkTargetFromAttackTargetIfTargetOutOfReach.create(AxolotlAi::getSpeedModifierChasing), MeleeAttack.create(20),
            EraseMemoryIf.<Axolotl>create(BehaviorUtils::isBreeding, MemoryModuleType.ATTACK_TARGET)), MemoryModuleType.ATTACK_TARGET);
    }

    protected static ActivityData<Axolotl> initCoreActivity() {
        return ActivityData.<Axolotl>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super Axolotl>>of(new LookAtTargetSink(45, 90), new MoveToTargetSink(),
            ValidatePlayDead.create(), new CountDownCooldownTicks(MemoryModuleType.TEMPTATION_COOLDOWN_TICKS)));
    }

    protected static ActivityData<Axolotl> initIdleActivity() {
        return ActivityData.<Axolotl>create(Activity.IDLE, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Axolotl>>>of(
            Pair.of(0, SetEntityLookTargetSometimes.create(EntityTypes.PLAYER, 6.0F, UniformInt.of(30, 60))),
            Pair.of(1, new AnimalMakeLove(EntityTypes.AXOLOTL, SPEED_MULTIPLIER_WHEN_MAKING_LOVE, 2)),
            Pair.of(2, new RunOne<Axolotl>(ImmutableList.of(Pair.of(new FollowTemptation(AxolotlAi::getSpeedModifier), 1),
                Pair.of(BabyFollowAdult.create(ADULT_FOLLOW_RANGE, AxolotlAi::getSpeedModifierFollowingAdult, MemoryModuleType.NEAREST_VISIBLE_ADULT, false), 1)))),
            Pair.of(3, StartAttacking.<Axolotl>create(AxolotlAi::findNearestValidAttackTarget)),
            Pair.of(3, TryFindLiquid.create(6, SPEED_MULTIPLIER_ON_LAND, FluidTags.AXOLOTL_TRIES_TO_FIND)),
            Pair.of(4, new GateBehavior<Axolotl>(Map.of(MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT), Set.of(), GateBehavior.OrderPolicy.ORDERED,
                GateBehavior.RunningPolicy.TRY_ALL, ImmutableList.of(Pair.of(RandomStroll.swim(SPEED_MULTIPLIER_WHEN_IDLING_IN_WATER), 2),
                    Pair.of(RandomStroll.stroll(SPEED_MULTIPLIER_ON_LAND, false), 2),
                    Pair.of(SetWalkTargetFromLookTarget.create(AxolotlAi::canSetWalkTargetFromLookTarget, AxolotlAi::getSpeedModifier, 3), 3),
                    Pair.of(BehaviorBuilder.<Axolotl>triggerIf(Entity::isInWater), 5), Pair.of(BehaviorBuilder.<Axolotl>triggerIf(Entity::onGround), 5))))));
    }

    private static boolean canSetWalkTargetFromLookTarget(LivingEntity body) {
        Optional<PositionTracker> lookTarget = body.getBrain().getMemory(MemoryModuleType.LOOK_TARGET);
        if (lookTarget.isEmpty()) {
            return false;
        }
        BlockPos pos = lookTarget.get().currentBlockPosition();
        return body.level().isWaterAt(pos) == body.isInWater();
    }

    public static void updateActivity(Axolotl body) {
        Brain<Axolotl> brain = body.getBrain();
        Activity oldActivity = brain.getActiveNonCoreActivity().orElse(null);
        if (oldActivity != Activity.PLAY_DEAD) {
            brain.setActiveActivityToFirstValid(List.of(Activity.PLAY_DEAD, Activity.FIGHT, Activity.IDLE));
            if (oldActivity == Activity.FIGHT && brain.getActiveNonCoreActivity().orElse(null) != Activity.FIGHT) {
                brain.setMemoryWithExpiry(MemoryModuleType.HAS_HUNTING_COOLDOWN, true, 2400L);
            }
        }
    }

    private static float getSpeedModifierChasing(LivingEntity mob) {
        return mob.isInWater() ? SPEED_MULTIPLIER_WHEN_CHASING_IN_WATER : SPEED_MULTIPLIER_ON_LAND;
    }

    private static float getSpeedModifierFollowingAdult(LivingEntity mob) {
        return mob.isInWater() ? SPEED_MULTIPLIER_WHEN_FOLLOWING_ADULT_IN_WATER : SPEED_MULTIPLIER_ON_LAND;
    }

    private static float getSpeedModifier(LivingEntity mob) {
        return mob.isInWater() ? SPEED_MULTIPLIER_WHEN_IDLING_IN_WATER : SPEED_MULTIPLIER_ON_LAND;
    }

    private static Optional<? extends LivingEntity> findNearestValidAttackTarget(ServerLevel level, Axolotl body) {
        return BehaviorUtils.isBreeding(body) ? Optional.empty() : body.getBrain().getMemory(MemoryModuleType.NEAREST_ATTACKABLE);
    }
}
