// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.piglin;

import java.util.Set;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.behavior.BehaviorControl;
import net.minecraft.world.entity.ai.behavior.OneShots;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class RememberIfHoglinWasKilled {
    public static BehaviorControl<LivingEntity> create() {
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.ATTACK_TARGET, MemoryModuleType.HUNTED_RECENTLY), (level, body, timestamp) -> {
            LivingEntity target = body.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET).orElse(null);
            if (target == null) {
                return false;
            }
            if (target.is(EntityTypes.HOGLIN) && target.isDeadOrDying()) {
                body.getBrain().setMemoryWithExpiry(MemoryModuleType.HUNTED_RECENTLY, true, PiglinAi.TIME_BETWEEN_HUNTS.sample(body.level().getRandom()));
            }
            return true;
        });
    }
}
