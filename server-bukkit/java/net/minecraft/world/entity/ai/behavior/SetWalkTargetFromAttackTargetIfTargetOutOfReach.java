// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import java.util.function.Function;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;

public class SetWalkTargetFromAttackTargetIfTargetOutOfReach {
    public static BehaviorControl<Mob> create(float speedModifier) {
        return create(entity -> speedModifier);
    }

    public static BehaviorControl<Mob> create(Function<LivingEntity, Float> speedModifier) {
        return OneShots.of(Set.of(MemoryModuleType.WALK_TARGET, MemoryModuleType.LOOK_TARGET, MemoryModuleType.ATTACK_TARGET), (level, body, timestamp) -> {
            var brain = body.getBrain();
            var target = brain.getMemory(MemoryModuleType.ATTACK_TARGET);
            if (target.isEmpty() || body.isWithinMeleeAttackRange(target.get())) {
                return false;
            }
            brain.setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(target.get(), true));
            brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(new EntityTracker(target.get(), false), speedModifier.apply(body), 0));
            return true;
        });
    }
}
