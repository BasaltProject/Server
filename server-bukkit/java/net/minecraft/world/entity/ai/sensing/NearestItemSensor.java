// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;

import java.util.Comparator;
import java.util.List;
import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.item.ItemEntity;

public class NearestItemSensor extends Sensor<Mob> {
    public static final int MAX_DISTANCE_TO_WANTED_ITEM = 32;

    public Set<MemoryModuleType<?>> requires() {
        return Set.of(MemoryModuleType.NEAREST_VISIBLE_WANTED_ITEM);
    }

    protected void doTick(ServerLevel level, Mob body) {
        List<ItemEntity> items = level.getEntitiesOfClass(ItemEntity.class, body.getBoundingBox().inflate(32.0, 16.0, 32.0),
            item -> item.closerThan(body, 32.0) && body.wantsToPickUp(level, item.getItem()));
        items.sort(Comparator.comparingDouble(body::distanceToSqr));
        body.getBrain().setMemory(MemoryModuleType.NEAREST_VISIBLE_WANTED_ITEM,
            items.stream().filter(item -> body.hasLineOfSight(item)).findFirst());
    }
}
