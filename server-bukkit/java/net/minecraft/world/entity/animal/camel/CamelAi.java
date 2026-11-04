// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.camel;

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
import java.util.function.Predicate;

public class CamelAi {
    private static final float SPEED_MULTIPLIER_WHEN_PANICKING = 4.0F;
    private static final float SPEED_MULTIPLIER_WHEN_IDLING = 2.0F;
    private static final float SPEED_MULTIPLIER_WHEN_TEMPTED = 2.5F;
    private static final float SPEED_MULTIPLIER_WHEN_FOLLOWING_ADULT = 2.5F;
    private static final float SPEED_MULTIPLIER_WHEN_MAKING_LOVE = 1.0F;
    private static final UniformInt ADULT_FOLLOW_RANGE = UniformInt.of(5, 16);

    protected static void initMemories(Camel body, RandomSource random) {
    }

    protected static List<ActivityData<Camel>> getActivities() {
        return List.of(coreActivity(), idleActivity());
    }

    private static ActivityData<Camel> coreActivity() {
        return ActivityData.<Camel>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super Camel>>of(new Swim(0.8F), new CamelPanic(SPEED_MULTIPLIER_WHEN_PANICKING),
            new LookAtTargetSink(45, 90), new MoveToTargetSink(), new CountDownCooldownTicks(MemoryModuleType.TEMPTATION_COOLDOWN_TICKS),
            new CountDownCooldownTicks(MemoryModuleType.GAZE_COOLDOWN_TICKS)));
    }

    private static ActivityData<Camel> idleActivity() {
        Predicate<Camel> canMove = Predicate.not(Camel::refuseToMove);
        return ActivityData.<Camel>create(Activity.IDLE, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Camel>>>of(
            Pair.of(0, SetEntityLookTargetSometimes.create(EntityTypes.PLAYER, 6.0F, UniformInt.of(30, 60))),
            Pair.of(1, new AnimalMakeLove(EntityTypes.CAMEL, SPEED_MULTIPLIER_WHEN_MAKING_LOVE, 2)),
            Pair.of(2, new RunOne<Camel>(ImmutableList.of(
                Pair.of(new FollowTemptation(camel -> SPEED_MULTIPLIER_WHEN_TEMPTED, camel -> camel.isBaby() ? 2.5 : 3.5), 1),
                Pair.of(BehaviorBuilder.<Camel>triggerIf(canMove, BabyFollowAdult.create(ADULT_FOLLOW_RANGE, SPEED_MULTIPLIER_WHEN_FOLLOWING_ADULT)), 1)))),
            Pair.of(3, new RandomLookAround(UniformInt.of(150, 250), 30.0F, 0.0F, 0.0F)),
            Pair.of(4, new RunOne<Camel>(Map.of(MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT), ImmutableList.of(
                Pair.of(BehaviorBuilder.<Camel>triggerIf(canMove, RandomStroll.stroll(SPEED_MULTIPLIER_WHEN_IDLING)), 1),
                Pair.of(BehaviorBuilder.<Camel>triggerIf(canMove, SetWalkTargetFromLookTarget.create(SPEED_MULTIPLIER_WHEN_IDLING, 3)), 1),
                Pair.of(new RandomSitting(20), 1), Pair.of(new DoNothing(30, 60), 1))))));
    }

    public static void updateActivity(Camel body) {
        body.getBrain().setActiveActivityToFirstValid(List.of(Activity.IDLE));
    }

    public static class CamelPanic extends AnimalPanic<Camel> {
        public CamelPanic(float speedMultiplier) {
            super(speedMultiplier);
        }

        protected boolean checkExtraStartConditions(ServerLevel level, Camel body) {
            return super.checkExtraStartConditions(level, body) && !body.isMobControlled();
        }

        protected void start(ServerLevel level, Camel camel, long timestamp) {
            camel.standUpInstantly();
            super.start(level, camel, timestamp);
        }
    }

    public static class RandomSitting extends Behavior<Camel> {
        private final int minimalPoseTicks;

        public RandomSitting(int minimalPoseTimeSec) {
            super(Map.of());
            this.minimalPoseTicks = minimalPoseTimeSec * 20;
        }

        protected boolean checkExtraStartConditions(ServerLevel level, Camel body) {
            return !body.isInWater() && body.getPoseTime() >= minimalPoseTicks && !body.isLeashed() && body.onGround()
                && !body.hasControllingPassenger() && body.canCamelChangePose();
        }

        protected void start(ServerLevel level, Camel body, long timestamp) {
            if (body.isCamelSitting()) {
                body.standUp();
            } else if (!body.isPanicking()) {
                body.sitDown();
            }
        }
    }
}
