// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.breeze;

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
import java.util.function.BiPredicate;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.ai.sensing.Sensor;
import net.minecraft.util.Unit;

public class BreezeAi {
    public static final float SPEED_MULTIPLIER_WHEN_SLIDING = 0.6F;
    public static final float JUMP_CIRCLE_INNER_RADIUS = 4.0F;
    public static final float JUMP_CIRCLE_MIDDLE_RADIUS = 8.0F;
    public static final float JUMP_CIRCLE_OUTER_RADIUS = 24.0F;
    private static final int TICKS_TO_REMEMBER_SEEN_TARGET = 100;

    protected static List<ActivityData<Breeze>> getActivities(Breeze breeze) {
        return List.of(coreActivity(), idleActivity(), fightActivity(breeze));
    }

    private static ActivityData<Breeze> coreActivity() {
        return ActivityData.<Breeze>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super Breeze>>of(new Swim(0.8F), new LookAtTargetSink(45, 90)));
    }

    private static ActivityData<Breeze> idleActivity() {
        return ActivityData.<Breeze>create(Activity.IDLE, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Breeze>>>of(
            Pair.of(0, StartAttacking.<Breeze>create((level, breeze) -> breeze.getBrain().getMemory(MemoryModuleType.NEAREST_ATTACKABLE))),
            Pair.of(1, StartAttacking.<Breeze>create((level, breeze) -> breeze.getBrain().getMemory(MemoryModuleType.HURT_BY).map(DamageSource::getEntity)
                .filter(entity -> entity instanceof LivingEntity).map(entity -> (LivingEntity) entity))),
            Pair.of(2, new SlideToTargetSink(20, 40)),
            Pair.of(3, new RunOne<Breeze>(ImmutableList.of(Pair.of(new DoNothing(20, 100), 1), Pair.of(RandomStroll.stroll(SPEED_MULTIPLIER_WHEN_SLIDING), 2))))));
    }

    private static ActivityData<Breeze> fightActivity(Breeze body) {
        BiPredicate<ServerLevel, LivingEntity> notAttackable = Sensor.wasEntityAttackableLastNTicks(body, TICKS_TO_REMEMBER_SEEN_TARGET).negate();
        return ActivityData.<Breeze>create(Activity.FIGHT, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Breeze>>>of(
            Pair.of(0, StopAttackingIfTargetInvalid.<Breeze>create(notAttackable::test)), Pair.of(1, new Shoot()), Pair.of(2, new LongJump()),
            Pair.of(3, new ShootWhenStuck()), Pair.of(4, new Slide())),
            Set.of(Pair.of(MemoryModuleType.ATTACK_TARGET, MemoryStatus.VALUE_PRESENT), Pair.of(MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT)));
    }

    public static void updateActivity(Breeze body) {
        body.getBrain().setActiveActivityToFirstValid(List.of(Activity.FIGHT, Activity.IDLE));
    }

    public static class SlideToTargetSink extends MoveToTargetSink {
        public SlideToTargetSink(int minTimeout, int maxTimeout) {
            super(minTimeout, maxTimeout);
        }

        protected void start(ServerLevel level, Mob body, long timestamp) {
            super.start(level, body, timestamp);
            body.playSound(SoundEvents.BREEZE_SLIDE);
            body.setPose(Pose.SLIDING);
        }

        protected void stop(ServerLevel level, Mob body, long timestamp) {
            super.stop(level, body, timestamp);
            body.setPose(Pose.STANDING);
            if (body.getBrain().hasMemoryValue(MemoryModuleType.ATTACK_TARGET)) {
                body.getBrain().setMemoryWithExpiry(MemoryModuleType.BREEZE_SHOOT, Unit.INSTANCE, 60L);
            }
        }
    }
}
