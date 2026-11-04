// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;

import java.util.Optional;
import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.NearestVisibleLivingEntities;

public abstract class NearestVisibleLivingEntitySensor extends Sensor<LivingEntity> {
    protected abstract boolean isMatchingEntity(ServerLevel level, LivingEntity body, LivingEntity mob);

    protected abstract MemoryModuleType<LivingEntity> getMemoryToSet();

    public Set<MemoryModuleType<?>> requires() {
        return Set.of(getMemoryToSet());
    }

    protected void doTick(ServerLevel level, LivingEntity body) {
        body.getBrain().setMemory(getMemoryToSet(),
            getVisibleEntities(body).flatMap(visible -> visible.findClosest(other -> isMatchingEntity(level, body, other))));
    }

    protected Optional<NearestVisibleLivingEntities> getVisibleEntities(LivingEntity body) {
        return body.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES);
    }
}
