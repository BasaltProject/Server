// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.piglin;

import java.util.Set;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.entity.ai.behavior.BehaviorControl;
import net.minecraft.world.entity.ai.behavior.OneShots;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class StopHoldingItemIfNoLongerAdmiring {
    public static BehaviorControl<Piglin> create() {
        return OneShots.<Piglin>of(Set.of(MemoryModuleType.ADMIRING_ITEM), (level, body, timestamp) -> {
            if (body.getBrain().hasMemoryValue(MemoryModuleType.ADMIRING_ITEM) || body.getOffhandItem().isEmpty() || body.getOffhandItem().has(DataComponents.BLOCKS_ATTACKS)) {
                return false;
            }
            PiglinAi.stopHoldingOffHandItem(level, body, true);
            return true;
        });
    }
}
