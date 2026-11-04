// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.piglin;

import java.util.Optional;
import java.util.Set;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.behavior.BehaviorControl;
import net.minecraft.world.entity.ai.behavior.OneShots;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.item.ItemEntity;

public class StopAdmiringIfItemTooFarAway<E extends Piglin> {
    public static BehaviorControl<LivingEntity> create(int maxDistanceToItem) {
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.ADMIRING_ITEM, MemoryModuleType.NEAREST_VISIBLE_WANTED_ITEM), (level, body, timestamp) -> {
            var brain = body.getBrain();
            if (!brain.hasMemoryValue(MemoryModuleType.ADMIRING_ITEM) || !body.getOffhandItem().isEmpty()) {
                return false;
            }
            Optional<ItemEntity> nearest = brain.getMemory(MemoryModuleType.NEAREST_VISIBLE_WANTED_ITEM);
            if (nearest.isPresent() && nearest.get().closerThan(body, maxDistanceToItem)) {
                return false;
            }
            brain.eraseMemory(MemoryModuleType.ADMIRING_ITEM);
            return true;
        });
    }
}
