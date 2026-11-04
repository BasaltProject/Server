// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;

import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.Unit;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class IsInWaterSensor extends Sensor<LivingEntity> {
    public Set<MemoryModuleType<?>> requires() {
        return Set.of(MemoryModuleType.IS_IN_WATER);
    }

    protected void doTick(ServerLevel level, LivingEntity body) {
        if (body.isInWater()) {
            body.getBrain().setMemory(MemoryModuleType.IS_IN_WATER, Unit.INSTANCE);
        } else {
            body.getBrain().eraseMemory(MemoryModuleType.IS_IN_WATER);
        }
    }
}
