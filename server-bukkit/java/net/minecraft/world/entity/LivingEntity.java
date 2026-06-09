package net.minecraft.world.entity;

import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.waypoints.WaypointTransmitter;
public class LivingEntity extends Entity implements Attackable, WaypointTransmitter {
    protected LivingEntity() {
    }

    LivingEntity(EntityType<?> type, ServerLevel level) {
        super(type, level);
    }
}
