// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import java.util.function.Predicate;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.MobCategory;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class SetEntityLookTarget {
    public static BehaviorControl<LivingEntity> create(MobCategory category, float maxDist) {
        return create(entity -> category.equals(entity.getType().getCategory()), maxDist);
    }

    public static OneShot<LivingEntity> create(EntityType<?> type, float maxDist) {
        return create(entity -> type.equals(entity.getType()), maxDist);
    }

    public static OneShot<LivingEntity> create(float maxDist) {
        return create(entity -> true, maxDist);
    }

    public static OneShot<LivingEntity> create(Predicate<LivingEntity> predicate, float maxDist) {
        float maxSqr = maxDist * maxDist;
        return OneShots.of(Set.of(MemoryModuleType.LOOK_TARGET, MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES), (level, body, timestamp) -> {
            var brain = body.getBrain();
            if (brain.hasMemoryValue(MemoryModuleType.LOOK_TARGET)) {
                return false;
            }
            var found = brain.getMemory(MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES)
                .flatMap(visible -> visible.findClosest(entity -> predicate.test(entity) && entity.distanceToSqr(body) <= maxSqr && !entity.hasPassenger(body)));
            if (found.isEmpty()) {
                return false;
            }
            brain.setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(found.get(), true));
            return true;
        });
    }
}
