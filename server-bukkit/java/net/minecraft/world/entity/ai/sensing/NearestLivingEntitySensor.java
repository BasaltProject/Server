// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;

import java.util.Comparator;
import java.util.List;
import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.NearestVisibleLivingEntities;
import net.minecraft.world.phys.AABB;

public class NearestLivingEntitySensor<T extends LivingEntity> extends Sensor<T> {
    protected void doTick(ServerLevel level, T body) {
        double radius = body.getAttributeValue(Attributes.FOLLOW_RANGE);
        AABB area = body.getBoundingBox().inflate(radius, radius, radius);
        List<LivingEntity> found = level.getEntitiesOfClass(LivingEntity.class, area, other -> other != body && other.isAlive());
        found.sort(Comparator.comparingDouble(body::distanceToSqr));
        body.getBrain().setMemory(MemoryModuleType.NEAREST_LIVING_ENTITIES, found);
        body.getBrain().setMemory(MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES, new NearestVisibleLivingEntities(level, body, found));
    }

    public Set<MemoryModuleType<?>> requires() {
        return Set.of(MemoryModuleType.NEAREST_LIVING_ENTITIES, MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES);
    }
}
