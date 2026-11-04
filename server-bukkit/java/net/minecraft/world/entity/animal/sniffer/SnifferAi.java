// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.sniffer;

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
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.behavior.*;
import net.minecraft.world.entity.ai.behavior.declarative.BehaviorBuilder;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.core.BlockPos;
import net.minecraft.util.Unit;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.animal.Animal;

public class SnifferAi {
    private static final int MAX_LOOK_DISTANCE = 6;
    private static final int SNIFFING_COOLDOWN_TICKS = 9600;
    private static final float SPEED_MULTIPLIER_WHEN_IDLING = 1.0F;
    private static final float SPEED_MULTIPLIER_WHEN_PANICKING = 2.0F;
    private static final float SPEED_MULTIPLIER_WHEN_SNIFFING = 1.25F;
    private static final float SPEED_MULTIPLIER_WHEN_TEMPTED = 1.25F;

    public static List<ActivityData<Sniffer>> getActivities() {
        return List.of(coreActivity(), idleActivity(), sniffingActivity(), digActivity());
    }

    private static Sniffer resetSniffing(Sniffer body) {
        Brain<Sniffer> mind = body.getBrain();
        mind.eraseMemory(MemoryModuleType.SNIFFER_SNIFFING_TARGET);
        mind.eraseMemory(MemoryModuleType.SNIFFER_DIGGING);
        return body.transitionTo(Sniffer.State.IDLING);
    }

    private static ActivityData<Sniffer> coreActivity() {
        return ActivityData.<Sniffer>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super Sniffer>>of(new Swim(0.8F), new AnimalPanic<Sniffer>(SPEED_MULTIPLIER_WHEN_PANICKING) {
            protected void start(ServerLevel level, Sniffer body, long timestamp) {
                SnifferAi.resetSniffing(body);
                super.start(level, body, timestamp);
            }
        }, new MoveToTargetSink(500, 700), new CountDownCooldownTicks(MemoryModuleType.TEMPTATION_COOLDOWN_TICKS)));
    }

    private static ActivityData<Sniffer> sniffingActivity() {
        return ActivityData.<Sniffer>create(Activity.SNIFF, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Sniffer>>>of(Pair.of(0, new Searching())),
            Set.of(Pair.of(MemoryModuleType.IS_PANICKING, MemoryStatus.VALUE_ABSENT), Pair.of(MemoryModuleType.SNIFFER_SNIFFING_TARGET, MemoryStatus.VALUE_PRESENT),
                Pair.of(MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_PRESENT)));
    }

    private static ActivityData<Sniffer> digActivity() {
        return ActivityData.<Sniffer>create(Activity.DIG, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Sniffer>>>of(Pair.of(0, new Digging(160, 180)),
            Pair.of(0, new FinishedDigging(40))), Set.of(Pair.of(MemoryModuleType.IS_PANICKING, MemoryStatus.VALUE_ABSENT),
                Pair.of(MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT), Pair.of(MemoryModuleType.SNIFFER_DIGGING, MemoryStatus.VALUE_PRESENT)));
    }

    private static ActivityData<Sniffer> idleActivity() {
        return ActivityData.<Sniffer>create(Activity.IDLE, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Sniffer>>>of(
            Pair.of(0, new AnimalMakeLove(EntityTypes.SNIFFER) {
                protected void start(ServerLevel level, Animal body, long timestamp) {
                    SnifferAi.resetSniffing((Sniffer) body);
                    super.start(level, body, timestamp);
                }
            }),
            Pair.of(1, new FollowTemptation(sniffer -> SPEED_MULTIPLIER_WHEN_TEMPTED, sniffer -> sniffer.isBaby() ? 2.5 : 3.5) {
                protected void start(ServerLevel level, PathfinderMob body, long timestamp) {
                    SnifferAi.resetSniffing((Sniffer) body);
                    super.start(level, body, timestamp);
                }
            }),
            Pair.of(2, new LookAtTargetSink(45, 90)), Pair.of(3, new FeelingHappy(40, 100)),
            Pair.of(4, new RunOne<Sniffer>(ImmutableList.of(Pair.of(SetWalkTargetFromLookTarget.create(SPEED_MULTIPLIER_WHEN_IDLING, 3), 2),
                Pair.of(new Scenting(40, 80), 1), Pair.of(new Sniffing(40, 80), 1), Pair.of(SetEntityLookTarget.create(EntityTypes.PLAYER, MAX_LOOK_DISTANCE), 1),
                Pair.of(RandomStroll.stroll(SPEED_MULTIPLIER_WHEN_IDLING), 1), Pair.of(new DoNothing(5, 20), 2))))),
            Set.of(Pair.of(MemoryModuleType.SNIFFER_DIGGING, MemoryStatus.VALUE_ABSENT)));
    }

    public static void updateActivity(Sniffer body) {
        body.getBrain().setActiveActivityToFirstValid(List.of(Activity.DIG, Activity.SNIFF, Activity.IDLE));
    }

    private abstract static class Posed extends Behavior<Sniffer> {
        private final Sniffer.State pose;

        Posed(Map<MemoryModuleType<?>, MemoryStatus> conditions, int min, int max, Sniffer.State pose) {
            super(conditions, min, max);
            this.pose = pose;
        }

        @Override
        protected void start(ServerLevel level, Sniffer sniffer, long timestamp) {
            sniffer.transitionTo(pose);
        }

        @Override
        protected void stop(ServerLevel level, Sniffer sniffer, long timestamp) {
            boolean ranFullCourse = timedOut(timestamp);
            sniffer.transitionTo(Sniffer.State.IDLING);
            afterPose(sniffer, ranFullCourse);
        }

        void afterPose(Sniffer sniffer, boolean ranFullCourse) {
        }
    }

    private static class Sniffing extends Posed {
        private Sniffing(int min, int max) {
            super(Map.of(MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT, MemoryModuleType.SNIFFER_SNIFFING_TARGET, MemoryStatus.VALUE_ABSENT,
                MemoryModuleType.SNIFF_COOLDOWN, MemoryStatus.VALUE_ABSENT), min, max, Sniffer.State.SNIFFING);
        }

        protected boolean checkExtraStartConditions(ServerLevel level, Sniffer body) {
            return body.canSniff() && !body.isBaby();
        }

        protected boolean canStillUse(ServerLevel level, Sniffer body, long timestamp) {
            return body.canSniff();
        }

        @Override
        void afterPose(Sniffer sniffer, boolean ranFullCourse) {
            if (!ranFullCourse) {
                return;
            }
            Brain<Sniffer> mind = sniffer.getBrain();
            sniffer.calculateDigPosition().ifPresent(spot -> {
                mind.setMemory(MemoryModuleType.SNIFFER_SNIFFING_TARGET, spot);
                mind.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(spot, SPEED_MULTIPLIER_WHEN_SNIFFING, 0));
            });
        }
    }

    private static class Searching extends Behavior<Sniffer> {
        private Searching() {
            super(Map.of(MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_PRESENT, MemoryModuleType.IS_PANICKING, MemoryStatus.VALUE_ABSENT,
                MemoryModuleType.SNIFFER_SNIFFING_TARGET, MemoryStatus.VALUE_PRESENT), 600);
        }

        protected boolean checkExtraStartConditions(ServerLevel level, Sniffer sniffer) {
            return sniffer.canSniff();
        }

        protected boolean canStillUse(ServerLevel level, Sniffer sniffer, long timestamp) {
            if (!sniffer.canSniff()) {
                sniffer.transitionTo(Sniffer.State.IDLING);
                return false;
            }
            Optional<BlockPos> walkTarget = sniffer.getBrain().getMemory(MemoryModuleType.WALK_TARGET).map(WalkTarget::getTarget).map(PositionTracker::currentBlockPosition);
            Optional<BlockPos> sniffingTarget = sniffer.getBrain().getMemory(MemoryModuleType.SNIFFER_SNIFFING_TARGET);
            return walkTarget.isPresent() && sniffingTarget.isPresent() && sniffingTarget.get().equals(walkTarget.get());
        }

        protected void start(ServerLevel level, Sniffer sniffer, long timestamp) {
            sniffer.transitionTo(Sniffer.State.SEARCHING);
        }

        protected void stop(ServerLevel level, Sniffer sniffer, long timestamp) {
            if (sniffer.canDig() && sniffer.canSniff()) {
                sniffer.getBrain().setMemory(MemoryModuleType.SNIFFER_DIGGING, true);
            }
            sniffer.getBrain().eraseMemory(MemoryModuleType.WALK_TARGET);
            sniffer.getBrain().eraseMemory(MemoryModuleType.SNIFFER_SNIFFING_TARGET);
        }
    }

    private static class Digging extends Behavior<Sniffer> {
        private Digging(int min, int max) {
            super(Map.of(MemoryModuleType.IS_PANICKING, MemoryStatus.VALUE_ABSENT, MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT,
                MemoryModuleType.SNIFFER_DIGGING, MemoryStatus.VALUE_PRESENT, MemoryModuleType.SNIFF_COOLDOWN, MemoryStatus.VALUE_ABSENT), min, max);
        }

        protected boolean checkExtraStartConditions(ServerLevel level, Sniffer sniffer) {
            return sniffer.canSniff();
        }

        protected boolean canStillUse(ServerLevel level, Sniffer sniffer, long timestamp) {
            return sniffer.getBrain().getMemory(MemoryModuleType.SNIFFER_DIGGING).isPresent() && sniffer.canDig() && !sniffer.isInLove();
        }

        protected void start(ServerLevel level, Sniffer sniffer, long timestamp) {
            sniffer.transitionTo(Sniffer.State.DIGGING);
        }

        protected void stop(ServerLevel level, Sniffer sniffer, long timestamp) {
            if (timedOut(timestamp)) {
                sniffer.getBrain().setMemoryWithExpiry(MemoryModuleType.SNIFF_COOLDOWN, Unit.INSTANCE, SNIFFING_COOLDOWN_TICKS);
            } else {
                SnifferAi.resetSniffing(sniffer);
            }
        }
    }

    private static class FinishedDigging extends Posed {
        private FinishedDigging(int duration) {
            super(Map.of(MemoryModuleType.IS_PANICKING, MemoryStatus.VALUE_ABSENT, MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT,
                MemoryModuleType.SNIFFER_DIGGING, MemoryStatus.VALUE_PRESENT, MemoryModuleType.SNIFF_COOLDOWN, MemoryStatus.VALUE_PRESENT),
                duration, duration, Sniffer.State.RISING);
        }

        protected boolean checkExtraStartConditions(ServerLevel level, Sniffer sniffer) {
            return true;
        }

        protected boolean canStillUse(ServerLevel level, Sniffer sniffer, long timestamp) {
            return sniffer.getBrain().getMemory(MemoryModuleType.SNIFFER_DIGGING).isPresent();
        }

        @Override
        void afterPose(Sniffer sniffer, boolean ranFullCourse) {
            sniffer.onDiggingComplete(ranFullCourse);
            sniffer.getBrain().eraseMemory(MemoryModuleType.SNIFFER_DIGGING);
            sniffer.getBrain().setMemory(MemoryModuleType.SNIFFER_HAPPY, true);
        }
    }

    private static class FeelingHappy extends Posed {
        private FeelingHappy(int min, int max) {
            super(Map.of(MemoryModuleType.SNIFFER_HAPPY, MemoryStatus.VALUE_PRESENT), min, max, Sniffer.State.FEELING_HAPPY);
        }

        protected boolean canStillUse(ServerLevel level, Sniffer sniffer, long timestamp) {
            return true;
        }

        @Override
        void afterPose(Sniffer sniffer, boolean ranFullCourse) {
            sniffer.getBrain().eraseMemory(MemoryModuleType.SNIFFER_HAPPY);
        }
    }

    private static class Scenting extends Posed {
        private Scenting(int min, int max) {
            super(Map.of(MemoryModuleType.IS_PANICKING, MemoryStatus.VALUE_ABSENT, MemoryModuleType.SNIFFER_DIGGING, MemoryStatus.VALUE_ABSENT,
                MemoryModuleType.SNIFFER_SNIFFING_TARGET, MemoryStatus.VALUE_ABSENT, MemoryModuleType.SNIFFER_HAPPY, MemoryStatus.VALUE_ABSENT,
                MemoryModuleType.BREED_TARGET, MemoryStatus.VALUE_ABSENT), min, max, Sniffer.State.SCENTING);
        }

        protected boolean checkExtraStartConditions(ServerLevel level, Sniffer sniffer) {
            return !sniffer.isTempted();
        }

        protected boolean canStillUse(ServerLevel level, Sniffer sniffer, long timestamp) {
            return true;
        }
    }
}
