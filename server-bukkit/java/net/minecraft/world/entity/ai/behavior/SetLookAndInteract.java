// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Optional;
import java.util.Set;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class SetLookAndInteract {
    public static BehaviorControl<LivingEntity> create(EntityType<?> type, int interactionRange) {
        int rangeSqr = interactionRange * interactionRange;
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.LOOK_TARGET, MemoryModuleType.INTERACTION_TARGET, MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES),
            (level, body, timestamp) -> {
                var brain = body.getBrain();
                if (brain.hasMemoryValue(MemoryModuleType.INTERACTION_TARGET)) {
                    return false;
                }
                var entities = brain.getMemory(MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES).orElse(null);
                if (entities == null) {
                    return false;
                }
                Optional<LivingEntity> closest = entities.findClosest(e -> e.distanceToSqr(body) <= rangeSqr && e.is(type));
                if (closest.isEmpty()) {
                    return false;
                }
                brain.setMemory(MemoryModuleType.INTERACTION_TARGET, closest.get());
                brain.setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(closest.get(), true));
                return true;
            });
    }
}
