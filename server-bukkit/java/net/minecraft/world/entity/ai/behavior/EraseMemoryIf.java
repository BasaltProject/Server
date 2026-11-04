// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import java.util.function.Predicate;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class EraseMemoryIf {
    public static <E extends LivingEntity> BehaviorControl<E> create(Predicate<E> predicate, MemoryModuleType<?> memoryType) {
        return OneShots.of(Set.of(memoryType), (level, body, timestamp) -> {
            if (body.getBrain().hasMemoryValue(memoryType) && predicate.test((E) body)) {
                body.getBrain().eraseMemory(memoryType);
                return true;
            }
            return false;
        });
    }
}
