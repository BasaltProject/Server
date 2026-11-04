// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class BecomePassiveIfMemoryPresent {
    public static BehaviorControl<LivingEntity> create(MemoryModuleType<?> pacifyingMemory, int pacifyDuration) {
        return OneShots.of(Set.of(pacifyingMemory, MemoryModuleType.ATTACK_TARGET, MemoryModuleType.PACIFIED), (level, body, timestamp) -> {
            var brain = body.getBrain();
            if (!brain.hasMemoryValue(pacifyingMemory)) {
                return false;
            }
            brain.eraseMemory(MemoryModuleType.ATTACK_TARGET);
            brain.setMemoryWithExpiry(MemoryModuleType.PACIFIED, true, pacifyDuration);
            return true;
        });
    }
}
