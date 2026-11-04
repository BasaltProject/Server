// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior.warden;

import java.util.Optional;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.behavior.BehaviorControl;
import net.minecraft.world.entity.ai.behavior.BlockPosTracker;
import net.minecraft.world.entity.ai.behavior.OneShots;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class SetWardenLookTarget {
    public static BehaviorControl<LivingEntity> create() {
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.LOOK_TARGET, MemoryModuleType.DISTURBANCE_LOCATION, MemoryModuleType.ROAR_TARGET,
            MemoryModuleType.ATTACK_TARGET), (level, body, timestamp) -> {
            var brain = body.getBrain();
            if (brain.hasMemoryValue(MemoryModuleType.ATTACK_TARGET)) {
                return false;
            }
            Optional<BlockPos> target = brain.getMemory(MemoryModuleType.ROAR_TARGET).map(Entity::blockPosition).or(() -> brain.getMemory(MemoryModuleType.DISTURBANCE_LOCATION));
            if (target.isEmpty()) {
                return false;
            }
            brain.setMemory(MemoryModuleType.LOOK_TARGET, new BlockPosTracker(target.get()));
            return true;
        });
    }
}
