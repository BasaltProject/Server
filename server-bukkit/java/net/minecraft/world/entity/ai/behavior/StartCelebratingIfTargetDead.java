// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import java.util.function.BiPredicate;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.level.gamerules.GameRules;

public class StartCelebratingIfTargetDead {
    public static BehaviorControl<LivingEntity> create(int celebrateDuration, BiPredicate<LivingEntity, LivingEntity> dancePredicate) {
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.ATTACK_TARGET, MemoryModuleType.ANGRY_AT, MemoryModuleType.CELEBRATE_LOCATION, MemoryModuleType.DANCING),
            (level, body, timestamp) -> {
                var brain = body.getBrain();
                LivingEntity target = brain.getMemory(MemoryModuleType.ATTACK_TARGET).orElse(null);
                if (target == null || brain.hasMemoryValue(MemoryModuleType.CELEBRATE_LOCATION) || !target.isDeadOrDying()) {
                    return false;
                }
                if (dancePredicate.test(body, target)) {
                    brain.setMemoryWithExpiry(MemoryModuleType.DANCING, true, celebrateDuration);
                }
                brain.setMemoryWithExpiry(MemoryModuleType.CELEBRATE_LOCATION, target.blockPosition(), celebrateDuration);
                if (!target.is(EntityTypes.PLAYER) || level.getGameRules().get(GameRules.FORGIVE_DEAD_PLAYERS)) {
                    brain.eraseMemory(MemoryModuleType.ATTACK_TARGET);
                    brain.eraseMemory(MemoryModuleType.ANGRY_AT);
                }
                return true;
            });
    }
}
