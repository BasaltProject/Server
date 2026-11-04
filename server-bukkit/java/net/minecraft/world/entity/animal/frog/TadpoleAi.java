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

public class TadpoleAi {
    private static final float SPEED_MULTIPLIER_WHEN_PANICKING = 2.0F;
    private static final float SPEED_MULTIPLIER_WHEN_IDLING_IN_WATER = 0.5F;
    private static final float SPEED_MULTIPLIER_WHEN_TEMPTED = 1.25F;

    public static List<ActivityData<Tadpole>> getActivities() {
        return List.of(coreActivity(), idleActivity());
    }

    private static ActivityData<Tadpole> coreActivity() {
        return ActivityData.<Tadpole>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super Tadpole>>of(new AnimalPanic<>(SPEED_MULTIPLIER_WHEN_PANICKING),
            new LookAtTargetSink(45, 90), new MoveToTargetSink(), new CountDownCooldownTicks(MemoryModuleType.TEMPTATION_COOLDOWN_TICKS)));
    }

    private static ActivityData<Tadpole> idleActivity() {
        return ActivityData.<Tadpole>create(Activity.IDLE, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Tadpole>>>of(
            Pair.of(0, SetEntityLookTargetSometimes.create(EntityTypes.PLAYER, 6.0F, UniformInt.of(30, 60))),
            Pair.of(1, new FollowTemptation(mob -> SPEED_MULTIPLIER_WHEN_TEMPTED)),
            Pair.of(2, new GateBehavior<Tadpole>(Map.of(MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT), Set.of(), GateBehavior.OrderPolicy.ORDERED,
                GateBehavior.RunningPolicy.TRY_ALL, ImmutableList.of(Pair.of(RandomStroll.swim(SPEED_MULTIPLIER_WHEN_IDLING_IN_WATER), 2),
                    Pair.of(SetWalkTargetFromLookTarget.create(SPEED_MULTIPLIER_WHEN_IDLING_IN_WATER, 3), 3), Pair.of(BehaviorBuilder.<Tadpole>triggerIf(Entity::isInWater), 5))))));
    }

    public static void updateActivity(Tadpole body) {
        body.getBrain().setActiveActivityToFirstValid(List.of(Activity.IDLE));
    }
}
