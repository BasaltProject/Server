// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.NearestVisibleLivingEntities;
import net.minecraft.world.phys.Vec3;

public class EntityTracker implements PositionTracker {
    private final Entity entity;
    private final boolean trackEyeHeight;
    private final boolean targetEyeHeight;

    public EntityTracker(Entity entity, boolean trackEyeHeight) {
        this(entity, trackEyeHeight, false);
    }

    public EntityTracker(Entity entity, boolean trackEyeHeight, boolean targetEyeHeight) {
        this.entity = entity;
        this.trackEyeHeight = trackEyeHeight;
        this.targetEyeHeight = targetEyeHeight;
    }

    @Override
    public Vec3 currentPosition() {
        return trackEyeHeight ? entity.position().add(0.0, entity.getEyeHeight(), 0.0) : entity.position();
    }

    @Override
    public BlockPos currentBlockPosition() {
        return targetEyeHeight ? BlockPos.containing(entity.getEyePosition()) : entity.blockPosition();
    }

    @Override
    public boolean isVisibleBy(LivingEntity body) {
        if (!(entity instanceof LivingEntity living)) {
            return true;
        }
        if (!living.isAlive()) {
            return false;
        }
        Optional<NearestVisibleLivingEntities> visible = body.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES);
        return visible.isPresent() && visible.get().contains(living);
    }

    public Entity getEntity() {
        return entity;
    }

    @Override
    public String toString() {
        return "EntityTracker for " + entity;
    }
}
