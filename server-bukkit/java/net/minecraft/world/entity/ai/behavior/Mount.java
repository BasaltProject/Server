// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;

public class Mount {
    private static final int CLOSE_ENOUGH_TO_START_RIDING_DIST = 1;

    public static BehaviorControl<LivingEntity> create(float speedModifier) {
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.LOOK_TARGET, MemoryModuleType.WALK_TARGET, MemoryModuleType.RIDE_TARGET), (level, body, timestamp) -> {
            var brain = body.getBrain();
            Entity ridable = brain.getMemory(MemoryModuleType.RIDE_TARGET).orElse(null);
            if (ridable == null || brain.hasMemoryValue(MemoryModuleType.WALK_TARGET) || body.isPassenger()) {
                return false;
            }
            if (ridable.closerThan(body, CLOSE_ENOUGH_TO_START_RIDING_DIST)) {
                body.startRiding(ridable);
            } else {
                brain.setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(ridable, true));
                brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(new EntityTracker(ridable, false), speedModifier, 1));
            }
            return true;
        });
    }
}
