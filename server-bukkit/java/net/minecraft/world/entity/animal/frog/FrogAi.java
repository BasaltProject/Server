// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.frog;

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
import net.minecraft.sounds.SoundEvents;
import net.minecraft.tags.BlockTags;
import net.minecraft.tags.FluidTags;
import net.minecraft.tags.ItemTags;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.pathfinder.PathType;
import net.minecraft.world.level.pathfinder.PathfindingContext;
import net.minecraft.world.level.pathfinder.WalkNodeEvaluator;

public class FrogAi {
    private static final float SPEED_MULTIPLIER_WHEN_PANICKING = 2.0F;
    private static final float SPEED_MULTIPLIER_WHEN_IDLING = 1.0F;
    private static final float SPEED_MULTIPLIER_ON_LAND = 1.0F;
    private static final float SPEED_MULTIPLIER_IN_WATER = 0.75F;
    private static final UniformInt TIME_BETWEEN_LONG_JUMPS = UniformInt.of(100, 140);
    private static final int MAX_LONG_JUMP_HEIGHT = 2;
    private static final int MAX_LONG_JUMP_WIDTH = 4;
    private static final float MAX_JUMP_VELOCITY_MULTIPLIER = 3.5714288F;
    private static final float SPEED_MULTIPLIER_WHEN_TEMPTED = 1.25F;

    protected static void initMemories(Frog body, RandomSource random) {
        body.getBrain().setMemory(MemoryModuleType.LONG_JUMP_COOLDOWN_TICKS, TIME_BETWEEN_LONG_JUMPS.sample(random));
    }

    protected static List<ActivityData<Frog>> getActivities() {
        return List.of(coreActivity(), idleActivity(), swimActivity(), laySpawnActivity(), tongueActivity(), jumpActivity());
    }

    private static ActivityData<Frog> coreActivity() {
        return ActivityData.<Frog>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super Frog>>of(new AnimalPanic<>(SPEED_MULTIPLIER_WHEN_PANICKING),
            new LookAtTargetSink(45, 90), new MoveToTargetSink(), new CountDownCooldownTicks(MemoryModuleType.TEMPTATION_COOLDOWN_TICKS),
            new CountDownCooldownTicks(MemoryModuleType.LONG_JUMP_COOLDOWN_TICKS)));
    }

    private static BehaviorControl<Frog> startAttacking() {
        return StartAttacking.<Frog>create((level, body) -> canAttack(body), (level, body) -> body.getBrain().getMemory(MemoryModuleType.NEAREST_ATTACKABLE));
    }

    private static ActivityData<Frog> idleActivity() {
        return ActivityData.<Frog>create(Activity.IDLE, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Frog>>>of(
            Pair.of(0, SetEntityLookTargetSometimes.create(EntityTypes.PLAYER, 6.0F, UniformInt.of(30, 60))), Pair.of(0, new AnimalMakeLove(EntityTypes.FROG)),
            Pair.of(1, new FollowTemptation(s -> SPEED_MULTIPLIER_WHEN_TEMPTED)), Pair.of(2, startAttacking()), Pair.of(3, TryFindLand.create(6, SPEED_MULTIPLIER_ON_LAND)),
            Pair.of(4, new RunOne<Frog>(Map.of(MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT), ImmutableList.of(
                Pair.of(RandomStroll.stroll(SPEED_MULTIPLIER_WHEN_IDLING), 1), Pair.of(SetWalkTargetFromLookTarget.create(SPEED_MULTIPLIER_WHEN_IDLING, 3), 1),
                Pair.of(new Croak(), 3), Pair.of(BehaviorBuilder.<Frog>triggerIf(Entity::onGround), 2))))),
            Set.of(Pair.of(MemoryModuleType.LONG_JUMP_MID_JUMP, MemoryStatus.VALUE_ABSENT), Pair.of(MemoryModuleType.IS_IN_WATER, MemoryStatus.VALUE_ABSENT)));
    }

    private static ActivityData<Frog> swimActivity() {
        return ActivityData.<Frog>create(Activity.SWIM, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Frog>>>of(
            Pair.of(0, SetEntityLookTargetSometimes.create(EntityTypes.PLAYER, 6.0F, UniformInt.of(30, 60))),
            Pair.of(1, new FollowTemptation(s -> SPEED_MULTIPLIER_WHEN_TEMPTED)), Pair.of(2, startAttacking()), Pair.of(3, TryFindLand.create(8, 1.5F)),
            Pair.of(5, new GateBehavior<Frog>(Map.of(MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT), Set.of(), GateBehavior.OrderPolicy.ORDERED,
                GateBehavior.RunningPolicy.TRY_ALL, ImmutableList.of(Pair.of(RandomStroll.swim(SPEED_MULTIPLIER_IN_WATER), 1), Pair.of(RandomStroll.stroll(1.0F, true), 1),
                    Pair.of(SetWalkTargetFromLookTarget.create(1.0F, 3), 1), Pair.of(BehaviorBuilder.<Frog>triggerIf(Entity::isInWater), 5))))),
            Set.of(Pair.of(MemoryModuleType.LONG_JUMP_MID_JUMP, MemoryStatus.VALUE_ABSENT), Pair.of(MemoryModuleType.IS_IN_WATER, MemoryStatus.VALUE_PRESENT)));
    }

    private static ActivityData<Frog> laySpawnActivity() {
        return ActivityData.<Frog>create(Activity.LAY_SPAWN, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Frog>>>of(
            Pair.of(0, SetEntityLookTargetSometimes.create(EntityTypes.PLAYER, 6.0F, UniformInt.of(30, 60))), Pair.of(1, startAttacking()),
            Pair.of(2, TryFindLandNearLiquid.create(8, 1.0F, FluidTags.FROG_TRIES_TO_FIND_LAND_NEAR)), Pair.of(3, TryLaySpawnOnFluidNearLand.create(Blocks.FROGSPAWN)),
            Pair.of(4, new RunOne<Frog>(ImmutableList.of(Pair.of(RandomStroll.stroll(1.0F), 2), Pair.of(SetWalkTargetFromLookTarget.create(1.0F, 3), 1),
                Pair.of(new Croak(), 2), Pair.of(BehaviorBuilder.<Frog>triggerIf(Entity::onGround), 1))))),
            Set.of(Pair.of(MemoryModuleType.LONG_JUMP_MID_JUMP, MemoryStatus.VALUE_ABSENT), Pair.of(MemoryModuleType.IS_PREGNANT, MemoryStatus.VALUE_PRESENT)));
    }

    private static ActivityData<Frog> jumpActivity() {
        return ActivityData.<Frog>create(Activity.LONG_JUMP, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Frog>>>of(
            Pair.of(0, new LongJumpMidJump(TIME_BETWEEN_LONG_JUMPS, SoundEvents.FROG_STEP)),
            Pair.of(1, new LongJumpToPreferredBlock<Frog>(TIME_BETWEEN_LONG_JUMPS, MAX_LONG_JUMP_HEIGHT, MAX_LONG_JUMP_WIDTH, MAX_JUMP_VELOCITY_MULTIPLIER,
                f -> SoundEvents.FROG_LONG_JUMP, BlockTags.FROG_PREFER_JUMP_TO, 0.5F, FrogAi::isAcceptableLandingSpot))),
            Set.of(Pair.of(MemoryModuleType.TEMPTING_PLAYER, MemoryStatus.VALUE_ABSENT), Pair.of(MemoryModuleType.BREED_TARGET, MemoryStatus.VALUE_ABSENT),
                Pair.of(MemoryModuleType.LONG_JUMP_COOLDOWN_TICKS, MemoryStatus.VALUE_ABSENT), Pair.of(MemoryModuleType.IS_IN_WATER, MemoryStatus.VALUE_ABSENT)));
    }

    private static ActivityData<Frog> tongueActivity() {
        return ActivityData.<Frog>create(Activity.TONGUE, 0, ImmutableList.<BehaviorControl<? super Frog>>of(StopAttackingIfTargetInvalid.<Frog>create(),
            new ShootTongue(SoundEvents.FROG_TONGUE, SoundEvents.FROG_EAT)), MemoryModuleType.ATTACK_TARGET);
    }

    private static <E extends Mob> boolean isAcceptableLandingSpot(E body, BlockPos targetPos) {
        Level level = body.level();
        BlockPos below = targetPos.below();
        if (!level.getFluidState(targetPos).isEmpty() || !level.getFluidState(below).isEmpty() || !level.getFluidState(targetPos.above()).isEmpty()) {
            return false;
        }
        BlockState state = level.getBlockState(targetPos);
        BlockState stateBelow = level.getBlockState(below);
        if (state.is(BlockTags.FROG_PREFER_JUMP_TO) || stateBelow.is(BlockTags.FROG_PREFER_JUMP_TO)) {
            return true;
        }
        PathfindingContext context = new PathfindingContext(level, body);
        PathType pathType = WalkNodeEvaluator.getPathTypeStatic(context, targetPos.mutable());
        PathType pathTypeBelow = WalkNodeEvaluator.getPathTypeStatic(context, below.mutable());
        return pathType != PathType.TRAPDOOR && (!state.isAir() || pathTypeBelow != PathType.TRAPDOOR) ? LongJumpToRandomPos.defaultAcceptableLandingSpot(body, targetPos) : true;
    }

    private static boolean canAttack(Mob mob) {
        return !BehaviorUtils.isBreeding(mob);
    }

    public static void updateActivity(Frog body) {
        body.getBrain().setActiveActivityToFirstValid(List.of(Activity.TONGUE, Activity.LAY_SPAWN, Activity.LONG_JUMP, Activity.SWIM, Activity.IDLE));
    }

    public static Predicate<ItemStack> getTemptations() {
        return item -> item.is(ItemTags.FROG_FOOD);
    }
}
