// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;

import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class HurtBySensor extends Sensor<LivingEntity> {
    public Set<MemoryModuleType<?>> requires() {
        return Set.of(MemoryModuleType.HURT_BY, MemoryModuleType.HURT_BY_ENTITY);
    }

    protected void doTick(ServerLevel level, LivingEntity body) {
        Brain<?> brain = body.getBrain();
        DamageSource source = body.getLastDamageSource();
        if (source != null) {
            brain.setMemory(MemoryModuleType.HURT_BY, source);
            if (source.getEntity() instanceof LivingEntity attacker) {
                brain.setMemory(MemoryModuleType.HURT_BY_ENTITY, attacker);
            }
        } else {
            brain.eraseMemory(MemoryModuleType.HURT_BY);
        }
        brain.getMemory(MemoryModuleType.HURT_BY_ENTITY).ifPresent(attacker -> {
            if (!attacker.isAlive() || attacker.level() != level) {
                brain.eraseMemory(MemoryModuleType.HURT_BY_ENTITY);
            }
        });
    }
}
