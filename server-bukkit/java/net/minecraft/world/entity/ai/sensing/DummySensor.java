// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;

import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class DummySensor extends Sensor<LivingEntity> {
    protected void doTick(ServerLevel level, LivingEntity body) {
        return;
    }

    public Set<MemoryModuleType<?>> requires() {
        return Set.of();
    }
}
