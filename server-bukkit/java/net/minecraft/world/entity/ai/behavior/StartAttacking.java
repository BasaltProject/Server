// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Optional;
import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class StartAttacking {
    public static <E extends Mob> BehaviorControl<E> create(TargetFinder<E> targetFinderFunction) {
        return create((level, body) -> true, targetFinderFunction);
    }

    public static <E extends Mob> BehaviorControl<E> create(StartAttackingCondition<E> canAttackPredicate, TargetFinder<E> targetFinderFunction) {
        return OneShots.of(Set.of(MemoryModuleType.ATTACK_TARGET), (level, body, timestamp) -> {
            if (body.getBrain().hasMemoryValue(MemoryModuleType.ATTACK_TARGET) || !canAttackPredicate.test(level, body)) {
                return false;
            }
            Optional<? extends LivingEntity> target = targetFinderFunction.get(level, body);
            if (target.isEmpty() || !body.canAttack(target.get())) {
                return false;
            }
            body.getBrain().setMemory(MemoryModuleType.ATTACK_TARGET, target.get());
            body.getBrain().eraseMemory(MemoryModuleType.WALK_TARGET);
            return true;
        });
    }

    public interface StartAttackingCondition<E> {
        boolean test(ServerLevel level, E body);
    }

    public interface TargetFinder<E> {
        Optional<? extends LivingEntity> get(ServerLevel level, E body);
    }
}
