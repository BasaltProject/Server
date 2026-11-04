// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class StopAttackingIfTargetInvalid {
    private static final int TIMEOUT_TO_GET_WITHIN_ATTACK_RANGE = 200;

    public static <E extends Mob> BehaviorControl<E> create(TargetErasedCallback<E> onTargetErased) {
        return create((level, target) -> false, onTargetErased, true);
    }

    public static <E extends Mob> BehaviorControl<E> create(StopAttackCondition stopAttackingWhen) {
        return create(stopAttackingWhen, (level, body, target) -> {
        }, true);
    }

    public static <E extends Mob> BehaviorControl<E> create() {
        return create((level, target) -> false, (level, body, target) -> {
        }, true);
    }

    public static <E extends Mob> BehaviorControl<E> create(StopAttackCondition stopAttackingWhen, TargetErasedCallback<E> onTargetErased,
            boolean canGrowTiredOfTryingToReachTarget) {
        return OneShots.of(Set.of(MemoryModuleType.ATTACK_TARGET, MemoryModuleType.CANT_REACH_WALK_TARGET_SINCE), (level, body, timestamp) -> {
            var brain = body.getBrain();
            LivingEntity target = brain.getMemory(MemoryModuleType.ATTACK_TARGET).orElse(null);
            if (target == null) {
                return false;
            }
            boolean tired = canGrowTiredOfTryingToReachTarget && brain.getMemory(MemoryModuleType.CANT_REACH_WALK_TARGET_SINCE)
                .map(since -> timestamp - since >= TIMEOUT_TO_GET_WITHIN_ATTACK_RANGE).orElse(false);
            if (!target.isAlive() || tired || stopAttackingWhen.test(level, target)) {
                onTargetErased.accept(level, body, target);
                brain.eraseMemory(MemoryModuleType.ATTACK_TARGET);
                return true;
            }
            return false;
        });
    }

    public interface StopAttackCondition {
        boolean test(ServerLevel level, LivingEntity target);
    }

    public interface TargetErasedCallback<E> {
        void accept(ServerLevel level, E body, LivingEntity target);
    }
}
