// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.armadillo;

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
import net.minecraft.tags.DamageTypeTags;

public class ArmadilloAi {
    private static final float SPEED_MULTIPLIER_WHEN_PANICKING = 2.0F;
    private static final float SPEED_MULTIPLIER_WHEN_IDLING = 1.0F;
    private static final float SPEED_MULTIPLIER_WHEN_TEMPTED = 1.25F;
    private static final float SPEED_MULTIPLIER_WHEN_FOLLOWING_ADULT = 1.25F;
    private static final float SPEED_MULTIPLIER_WHEN_MAKING_LOVE = 1.0F;
    private static final double DEFAULT_CLOSE_ENOUGH_DIST = 2.0;
    private static final double BABY_CLOSE_ENOUGH_DIST = 1.0;
    private static final UniformInt ADULT_FOLLOW_RANGE = UniformInt.of(5, 16);

    private static final OneShot<Armadillo> ARMADILLO_ROLLING_OUT = OneShots.<Armadillo>of(Set.of(MemoryModuleType.DANGER_DETECTED_RECENTLY), (level, body, timestamp) -> {
        if (body.getBrain().hasMemoryValue(MemoryModuleType.DANGER_DETECTED_RECENTLY) || !body.isScared()) {
            return false;
        }
        body.rollOut();
        return true;
    });

    protected static List<ActivityData<Armadillo>> getActivities() {
        return List.of(coreActivity(), idleActivity(), scaredActivity());
    }

    private static ActivityData<Armadillo> coreActivity() {
        return ActivityData.<Armadillo>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super Armadillo>>of(new Swim(0.8F),
            new ArmadilloPanic(SPEED_MULTIPLIER_WHEN_PANICKING), new LookAtTargetSink(45, 90), new MoveToTargetSink() {
                protected boolean checkExtraStartConditions(ServerLevel level, Mob body) {
                    return !(body instanceof Armadillo armadillo && armadillo.isScared()) && super.checkExtraStartConditions(level, body);
                }
            }, new CountDownCooldownTicks(MemoryModuleType.TEMPTATION_COOLDOWN_TICKS), new CountDownCooldownTicks(MemoryModuleType.GAZE_COOLDOWN_TICKS),
            ARMADILLO_ROLLING_OUT));
    }

    private static ActivityData<Armadillo> idleActivity() {
        return ActivityData.<Armadillo>create(Activity.IDLE, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Armadillo>>>of(
            Pair.of(0, SetEntityLookTargetSometimes.create(EntityTypes.PLAYER, 6.0F, UniformInt.of(30, 60))),
            Pair.of(1, new AnimalMakeLove(EntityTypes.ARMADILLO, SPEED_MULTIPLIER_WHEN_MAKING_LOVE, 1)),
            Pair.of(2, new RunOne<Armadillo>(ImmutableList.of(
                Pair.of(new FollowTemptation(a -> SPEED_MULTIPLIER_WHEN_TEMPTED, a -> a.isBaby() ? BABY_CLOSE_ENOUGH_DIST : DEFAULT_CLOSE_ENOUGH_DIST), 1),
                Pair.of(BabyFollowAdult.create(ADULT_FOLLOW_RANGE, SPEED_MULTIPLIER_WHEN_FOLLOWING_ADULT), 1)))),
            Pair.of(3, new RandomLookAround(UniformInt.of(150, 250), 30.0F, 0.0F, 0.0F)),
            Pair.of(4, new RunOne<Armadillo>(Map.of(MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT), ImmutableList.of(
                Pair.of(RandomStroll.stroll(SPEED_MULTIPLIER_WHEN_IDLING), 1), Pair.of(SetWalkTargetFromLookTarget.create(SPEED_MULTIPLIER_WHEN_IDLING, 3), 1),
                Pair.of(new DoNothing(30, 60), 1))))));
    }

    private static ActivityData<Armadillo> scaredActivity() {
        return ActivityData.<Armadillo>create(Activity.PANIC, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Armadillo>>>of(Pair.of(0, new ArmadilloBallUp())),
            Set.of(Pair.of(MemoryModuleType.DANGER_DETECTED_RECENTLY, MemoryStatus.VALUE_PRESENT), Pair.of(MemoryModuleType.IS_PANICKING, MemoryStatus.VALUE_ABSENT)));
    }

    public static void updateActivity(Armadillo body) {
        body.getBrain().setActiveActivityToFirstValid(List.of(Activity.PANIC, Activity.IDLE));
    }

    public static class ArmadilloBallUp extends Behavior<Armadillo> {
        private static final int BALL_UP_STAY_IN_STATE = 5 * 60 * 20;
        private static final int DANGER_DETECTED_RECENTLY_DANGER_THRESHOLD = 75;
        private int nextPeekTimer = 0;
        private boolean dangerWasAround;

        public ArmadilloBallUp() {
            super(Map.of(), BALL_UP_STAY_IN_STATE);
        }

        protected void tick(ServerLevel level, Armadillo body, long timestamp) {
            super.tick(level, body, timestamp);
            if (nextPeekTimer > 0) {
                --nextPeekTimer;
            }
            if (body.shouldSwitchToScaredState()) {
                body.switchToState(Armadillo.ArmadilloState.SCARED);
                return;
            }
            Armadillo.ArmadilloState current = body.getState();
            long remainingDanger = body.getBrain().getTimeUntilExpiry(MemoryModuleType.DANGER_DETECTED_RECENTLY);
            boolean threatened = remainingDanger > DANGER_DETECTED_RECENTLY_DANGER_THRESHOLD;
            if (threatened != dangerWasAround) {
                nextPeekTimer = pickNextPeekTimer(body);
            }
            dangerWasAround = threatened;
            long unrollTime = Armadillo.ArmadilloState.UNROLLING.animationDuration();
            switch (current) {
                case SCARED -> {
                    if (threatened && nextPeekTimer == 0 && body.onGround()) {
                        level.broadcastEntityEvent(body, (byte) 64);
                        nextPeekTimer = pickNextPeekTimer(body);
                    }
                    if (remainingDanger < unrollTime) {
                        body.switchToState(Armadillo.ArmadilloState.UNROLLING);
                    }
                }
                case UNROLLING -> {
                    if (remainingDanger > unrollTime) {
                        body.switchToState(Armadillo.ArmadilloState.SCARED);
                    }
                }
                default -> { }
            }
        }

        private int pickNextPeekTimer(Armadillo body) {
            return Armadillo.ArmadilloState.SCARED.animationDuration() + body.getRandom().nextIntBetweenInclusive(100, 400);
        }

        protected boolean checkExtraStartConditions(ServerLevel level, Armadillo body) {
            return body.onGround() && !body.isInLiquid();
        }

        protected boolean canStillUse(ServerLevel level, Armadillo body, long timestamp) {
            return body.getState().isThreatened();
        }

        protected void start(ServerLevel level, Armadillo body, long timestamp) {
            body.rollUp();
        }

        protected void stop(ServerLevel level, Armadillo body, long timestamp) {
            if (!body.canStayRolledUp()) {
                body.rollOut();
            }
        }
    }

    public static class ArmadilloPanic extends AnimalPanic<Armadillo> {
        public ArmadilloPanic(float speedMultiplier) {
            super(speedMultiplier, mob -> DamageTypeTags.PANIC_ENVIRONMENTAL_CAUSES);
        }

        protected void start(ServerLevel level, Armadillo armadillo, long timestamp) {
            armadillo.rollOut();
            super.start(level, armadillo, timestamp);
        }
    }
}
