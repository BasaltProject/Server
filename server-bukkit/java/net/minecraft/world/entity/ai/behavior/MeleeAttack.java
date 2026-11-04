// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import java.util.function.Predicate;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class MeleeAttack {
    public static <T extends Mob> OneShot<T> create(int cooldownBetweenAttacks) {
        return create(mob -> true, cooldownBetweenAttacks);
    }

    public static <T extends Mob> OneShot<T> create(Predicate<T> canAttackPredicate, int cooldownBetweenAttacks) {
        return OneShots.of(Set.of(MemoryModuleType.ATTACK_TARGET, MemoryModuleType.ATTACK_COOLING_DOWN, MemoryModuleType.LOOK_TARGET, MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES), (level, mob, timestamp) -> {
            var brain = mob.getBrain();
            var target = brain.getMemory(MemoryModuleType.ATTACK_TARGET);
            if (target.isEmpty() || brain.hasMemoryValue(MemoryModuleType.ATTACK_COOLING_DOWN)) {
                return false;
            }
            var victim = target.get();
            if (!canAttackPredicate.test((T) mob) || !mob.isWithinMeleeAttackRange(victim) || !BehaviorUtils.canSee(mob, victim)) {
                return false;
            }
            brain.setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(victim, true));
            mob.swing(InteractionHand.MAIN_HAND, net.minecraft.world.item.component.SwingAnimation.DEFAULT, true);
            mob.doHurtTarget(level, victim);
            brain.setMemoryWithExpiry(MemoryModuleType.ATTACK_COOLING_DOWN, true, cooldownBetweenAttacks);
            return true;
        });
    }
}
