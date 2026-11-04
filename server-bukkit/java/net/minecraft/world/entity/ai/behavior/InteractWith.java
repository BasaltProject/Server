// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Optional;
import java.util.Set;
import java.util.function.Predicate;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.NearestVisibleLivingEntities;
import net.minecraft.world.entity.ai.memory.WalkTarget;

public class InteractWith {
    public static <T extends LivingEntity> BehaviorControl<LivingEntity> of(EntityType<? extends T> type, int interactionRange, MemoryModuleType<T> interactionTarget,
            float speedModifier, int stopDistance) {
        return of(type, interactionRange, mob -> true, mob -> true, interactionTarget, speedModifier, stopDistance);
    }

    public static <E extends LivingEntity, T extends LivingEntity> BehaviorControl<E> of(EntityType<? extends T> type, int interactionRange, Predicate<E> selfFilter,
            Predicate<T> targetFilter, MemoryModuleType<T> interactionTarget, float speedModifier, int stopDistance) {
        int rangeSqr = interactionRange * interactionRange;
        Predicate<LivingEntity> isTargetValid = mob -> mob.is(type) && targetFilter.test((T) mob);
        return OneShots.<E>of(Set.of(interactionTarget, MemoryModuleType.LOOK_TARGET, MemoryModuleType.WALK_TARGET, MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES),
            (level, body, timestamp) -> {
                var brain = body.getBrain();
                NearestVisibleLivingEntities entities = brain.getMemory(MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES).orElse(null);
                if (entities == null || brain.hasMemoryValue(MemoryModuleType.WALK_TARGET) || !selfFilter.test(body) || !entities.contains(isTargetValid)) {
                    return false;
                }
                Optional<LivingEntity> closest = entities.findClosest(mob -> mob.distanceToSqr(body) <= rangeSqr && isTargetValid.test(mob));
                closest.ifPresent(mob -> {
                    brain.setMemory(interactionTarget, (T) mob);
                    brain.setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(mob, true));
                    brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(new EntityTracker(mob, false), speedModifier, stopDistance));
                });
                return true;
            });
    }
}
