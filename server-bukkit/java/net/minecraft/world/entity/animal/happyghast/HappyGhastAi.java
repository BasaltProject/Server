// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.happyghast;

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

public class HappyGhastAi {
    private static final float SPEED_MULTIPLIER_WHEN_IDLING = 1.0F;
    private static final float SPEED_MULTIPLIER_WHEN_TEMPTED = 1.25F;
    private static final float SPEED_MULTIPLIER_WHEN_FOLLOWING_ADULT = 1.1F;
    private static final double BABY_GHAST_CLOSE_ENOUGH_DIST = 3.0;
    private static final UniformInt ADULT_FOLLOW_RANGE = UniformInt.of(3, 16);

    protected static List<ActivityData<HappyGhast>> getActivities() {
        return List.of(coreActivity(), idleActivity(), panicActivity());
    }

    private static ActivityData<HappyGhast> coreActivity() {
        return ActivityData.<HappyGhast>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super HappyGhast>>of(new Swim(0.8F), new AnimalPanic<>(2.0F, 0),
            new LookAtTargetSink(45, 90), new MoveToTargetSink(), new CountDownCooldownTicks(MemoryModuleType.TEMPTATION_COOLDOWN_TICKS)));
    }

    private static ActivityData<HappyGhast> idleActivity() {
        return ActivityData.<HappyGhast>create(Activity.IDLE, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super HappyGhast>>>of(
            Pair.of(1, new FollowTemptation(mob -> SPEED_MULTIPLIER_WHEN_TEMPTED, mob -> BABY_GHAST_CLOSE_ENOUGH_DIST, true)),
            Pair.of(2, BabyFollowAdult.create(ADULT_FOLLOW_RANGE, mob -> SPEED_MULTIPLIER_WHEN_FOLLOWING_ADULT, MemoryModuleType.NEAREST_VISIBLE_PLAYER, true)),
            Pair.of(3, BabyFollowAdult.create(ADULT_FOLLOW_RANGE, mob -> SPEED_MULTIPLIER_WHEN_FOLLOWING_ADULT, MemoryModuleType.NEAREST_VISIBLE_ADULT, true)),
            Pair.of(4, new RunOne<HappyGhast>(ImmutableList.of(Pair.of(RandomStroll.fly(SPEED_MULTIPLIER_WHEN_IDLING), 1),
                Pair.of(SetWalkTargetFromLookTarget.create(SPEED_MULTIPLIER_WHEN_IDLING, 3), 1))))));
    }

    private static ActivityData<HappyGhast> panicActivity() {
        return ActivityData.<HappyGhast>create(Activity.PANIC, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super HappyGhast>>>of(),
            Set.of(Pair.of(MemoryModuleType.IS_PANICKING, MemoryStatus.VALUE_PRESENT)));
    }

    public static void updateActivity(HappyGhast body) {
        body.getBrain().setActiveActivityToFirstValid(List.of(Activity.PANIC, Activity.IDLE));
    }
}
