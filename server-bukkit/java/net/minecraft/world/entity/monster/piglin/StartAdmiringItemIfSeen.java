// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.piglin;

import java.util.Set;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.behavior.BehaviorControl;
import net.minecraft.world.entity.ai.behavior.OneShots;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.item.ItemEntity;

public class StartAdmiringItemIfSeen {
    public static BehaviorControl<LivingEntity> create(int admireDuration) {
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.NEAREST_VISIBLE_WANTED_ITEM, MemoryModuleType.ADMIRING_ITEM, MemoryModuleType.ADMIRING_DISABLED,
            MemoryModuleType.DISABLE_WALK_TO_ADMIRE_ITEM), (level, body, timestamp) -> {
            var brain = body.getBrain();
            ItemEntity item = brain.getMemory(MemoryModuleType.NEAREST_VISIBLE_WANTED_ITEM).orElse(null);
            if (item == null || brain.hasMemoryValue(MemoryModuleType.ADMIRING_ITEM) || brain.hasMemoryValue(MemoryModuleType.ADMIRING_DISABLED)
                || brain.hasMemoryValue(MemoryModuleType.DISABLE_WALK_TO_ADMIRE_ITEM) || !PiglinAi.isLovedItem(item.getItem())) {
                return false;
            }
            brain.setMemoryWithExpiry(MemoryModuleType.ADMIRING_ITEM, true, admireDuration);
            return true;
        });
    }
}
