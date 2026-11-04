// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import java.util.function.Function;
import java.util.function.Predicate;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;

public class SetWalkTargetFromLookTarget {
    public static OneShot<LivingEntity> create(float speedModifier, int closeEnoughDistance) {
        return create(entity -> true, entity -> speedModifier, closeEnoughDistance);
    }

    public static OneShot<LivingEntity> create(Predicate<LivingEntity> canSetWalkTargetPredicate, Function<LivingEntity, Float> speedModifier,
            int closeEnoughDistance) {
        return OneShots.of(Set.of(MemoryModuleType.LOOK_TARGET, MemoryModuleType.WALK_TARGET), (level, body, timestamp) -> {
            var look = body.getBrain().getMemory(MemoryModuleType.LOOK_TARGET);
            if (look.isEmpty() || !canSetWalkTargetPredicate.test(body)) {
                return false;
            }
            body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(look.get(), speedModifier.apply(body), closeEnoughDistance));
            return true;
        });
    }
}
