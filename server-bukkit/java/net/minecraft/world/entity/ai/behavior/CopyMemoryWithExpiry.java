// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import java.util.function.Predicate;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class CopyMemoryWithExpiry {
    public static <E extends LivingEntity, T> BehaviorControl<E> create(Predicate<E> copyIfTrue, MemoryModuleType<? extends T> sourceMemory,
            MemoryModuleType<T> targetMemory, UniformInt durationOfCopy) {
        return OneShots.of(Set.of(sourceMemory, targetMemory), (level, body, timestamp) -> {
            var value = body.getBrain().getMemory(sourceMemory);
            if (value.isEmpty() || !copyIfTrue.test((E) body)) {
                return false;
            }
            body.getBrain().setMemoryWithExpiry(targetMemory, value.get(), durationOfCopy.sample(level.getRandom()));
            return true;
        });
    }
}
