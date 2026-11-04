// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.axolotl;

import java.util.Set;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.behavior.BehaviorControl;
import net.minecraft.world.entity.ai.behavior.OneShots;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class ValidatePlayDead {
    public static BehaviorControl<LivingEntity> create() {
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.PLAY_DEAD_TICKS, MemoryModuleType.HURT_BY_ENTITY), (level, body, timestamp) -> {
            var brain = body.getBrain();
            Integer ticks = brain.getMemory(MemoryModuleType.PLAY_DEAD_TICKS).orElse(null);
            if (ticks == null) {
                return false;
            }
            if (ticks <= 0) {
                brain.eraseMemory(MemoryModuleType.PLAY_DEAD_TICKS);
                brain.eraseMemory(MemoryModuleType.HURT_BY_ENTITY);
                brain.useDefaultActivity();
            } else {
                brain.setMemory(MemoryModuleType.PLAY_DEAD_TICKS, ticks - 1);
            }
            return true;
        });
    }
}
