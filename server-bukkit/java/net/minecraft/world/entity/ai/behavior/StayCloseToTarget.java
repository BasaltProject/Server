// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Optional;
import java.util.Set;
import java.util.function.Function;
import java.util.function.Predicate;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;

public class StayCloseToTarget {
    public static BehaviorControl<LivingEntity> create(Function<LivingEntity, Optional<PositionTracker>> targetPositionGetter,
            Predicate<LivingEntity> shouldRunPredicate, int closeEnough, int tooFar, float speedModifier) {
        return OneShots.of(Set.of(MemoryModuleType.LOOK_TARGET, MemoryModuleType.WALK_TARGET), (level, body, timestamp) -> {
            Optional<PositionTracker> target = targetPositionGetter.apply(body);
            if (target.isEmpty() || !shouldRunPredicate.test(body)) {
                return false;
            }
            PositionTracker tracker = target.get();
            if (body.position().closerThan(tracker.currentPosition(), tooFar)) {
                return false;
            }
            body.getBrain().setMemory(MemoryModuleType.LOOK_TARGET, tracker);
            body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(tracker, speedModifier, closeEnough));
            return true;
        });
    }
}
