// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import java.util.function.Function;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;

public class BabyFollowAdult {
    public static OneShot<LivingEntity> create(UniformInt followRange, float speedModifier) {
        return create(followRange, mob -> speedModifier, MemoryModuleType.NEAREST_VISIBLE_ADULT, false);
    }

    public static OneShot<LivingEntity> create(UniformInt followRange, Function<LivingEntity, Float> speedModifier,
            MemoryModuleType<? extends LivingEntity> nearestVisibleType, boolean targetEye) {
        return OneShots.of(Set.of(nearestVisibleType, MemoryModuleType.LOOK_TARGET, MemoryModuleType.WALK_TARGET), (level, body, timestamp) -> {
            var brain = body.getBrain();
            if (!body.isBaby() || brain.hasMemoryValue(MemoryModuleType.WALK_TARGET)) {
                return false;
            }
            LivingEntity adult = brain.getMemory(nearestVisibleType).orElse(null);
            if (adult == null || !body.closerThan(adult, followRange.maxInclusive() + 1) || body.closerThan(adult, followRange.minInclusive())) {
                return false;
            }
            brain.setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(adult, true, targetEye));
            brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(new EntityTracker(adult, targetEye, targetEye), speedModifier.apply(body), followRange.minInclusive() - 1));
            return true;
        });
    }
}
