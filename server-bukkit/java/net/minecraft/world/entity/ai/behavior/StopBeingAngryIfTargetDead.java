// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class StopBeingAngryIfTargetDead {
    public static BehaviorControl<LivingEntity> create() {
        return OneShots.of(Set.of(MemoryModuleType.ANGRY_AT), (level, body, timestamp) -> {
            var target = BehaviorUtils.getLivingEntityFromUUIDMemory(body, MemoryModuleType.ANGRY_AT);
            if (target.isPresent() && !target.get().isAlive()) {
                body.getBrain().eraseMemory(MemoryModuleType.ANGRY_AT);
                return true;
            }
            return false;
        });
    }
}
