// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior.warden;

import java.util.Optional;
import java.util.Set;
import java.util.function.Function;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.behavior.BehaviorControl;
import net.minecraft.world.entity.ai.behavior.OneShots;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.monster.warden.Warden;

public class SetRoarTarget {
    public static <E extends Warden> BehaviorControl<E> create(Function<E, Optional<? extends LivingEntity>> targetFinderFunction) {
        return OneShots.<E>of(Set.of(MemoryModuleType.ROAR_TARGET, MemoryModuleType.ATTACK_TARGET, MemoryModuleType.CANT_REACH_WALK_TARGET_SINCE), (level, body, timestamp) -> {
            var brain = body.getBrain();
            if (brain.hasMemoryValue(MemoryModuleType.ROAR_TARGET) || brain.hasMemoryValue(MemoryModuleType.ATTACK_TARGET)) {
                return false;
            }
            Optional<? extends LivingEntity> target = targetFinderFunction.apply(body).filter(body::canTargetEntity);
            if (target.isEmpty()) {
                return false;
            }
            brain.setMemory(MemoryModuleType.ROAR_TARGET, target.get());
            brain.eraseMemory(MemoryModuleType.CANT_REACH_WALK_TARGET_SINCE);
            return true;
        });
    }
}
