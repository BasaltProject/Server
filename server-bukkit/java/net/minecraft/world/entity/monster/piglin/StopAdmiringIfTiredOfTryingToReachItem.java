// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.piglin;

import java.util.Optional;
import java.util.Set;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.behavior.BehaviorControl;
import net.minecraft.world.entity.ai.behavior.OneShots;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class StopAdmiringIfTiredOfTryingToReachItem {
    public static BehaviorControl<LivingEntity> create(int maxTimeToReachItem, int disableTime) {
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.ADMIRING_ITEM, MemoryModuleType.NEAREST_VISIBLE_WANTED_ITEM,
            MemoryModuleType.TIME_TRYING_TO_REACH_ADMIRE_ITEM, MemoryModuleType.DISABLE_WALK_TO_ADMIRE_ITEM), (level, body, timestamp) -> {
            var brain = body.getBrain();
            if (!brain.hasMemoryValue(MemoryModuleType.ADMIRING_ITEM) || !brain.hasMemoryValue(MemoryModuleType.NEAREST_VISIBLE_WANTED_ITEM) || !body.getOffhandItem().isEmpty()) {
                return false;
            }
            Optional<Integer> time = brain.getMemory(MemoryModuleType.TIME_TRYING_TO_REACH_ADMIRE_ITEM);
            if (time.isEmpty()) {
                brain.setMemory(MemoryModuleType.TIME_TRYING_TO_REACH_ADMIRE_ITEM, 0);
            } else if (time.get() > maxTimeToReachItem) {
                brain.eraseMemory(MemoryModuleType.ADMIRING_ITEM);
                brain.eraseMemory(MemoryModuleType.TIME_TRYING_TO_REACH_ADMIRE_ITEM);
                brain.setMemoryWithExpiry(MemoryModuleType.DISABLE_WALK_TO_ADMIRE_ITEM, true, disableTime);
            } else {
                brain.setMemory(MemoryModuleType.TIME_TRYING_TO_REACH_ADMIRE_ITEM, time.get() + 1);
            }
            return true;
        });
    }
}
