package org.basaltmc.bukkit;

import org.bukkit.Location;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Vehicle;

/** The vehicle events raised from deep in the game systems, as the Bukkit events plugins listen for. */
final class HookVehicleEvents {
    private HookVehicleEvents() {}

    static void install() {
        EventBridge.map("vehicle.move", org.bukkit.event.vehicle.VehicleMoveEvent.class, f ->
            f.entity() instanceof Vehicle vehicle ? new org.bukkit.event.vehicle.VehicleMoveEvent(vehicle,
                new Location(f.world(), f.real("from_x"), f.real("from_y"), f.real("from_z")),
                new Location(f.world(), f.real("x"), f.real("y"), f.real("z"))) : null);
        EventBridge.map("vehicle.entity_collision", org.bukkit.event.vehicle.VehicleEntityCollisionEvent.class, f -> {
            Entity other = BasaltServer.instance().entityById(f.world(), (int) f.whole("other_id"));
            return f.entity() instanceof Vehicle vehicle && other != null
                ? new org.bukkit.event.vehicle.VehicleEntityCollisionEvent(vehicle, other) : null;
        });
        EventBridge.map("entity.add", org.bukkit.event.vehicle.VehicleCreateEvent.class, f -> {
            Entity added = f.entity();
            if (added == null && f.world() != null) {
                org.bukkit.entity.EntityType type = org.basaltmc.bukkit.entity.EntityCore.typeOf(f.text("entity_type"));
                added = type == org.bukkit.entity.EntityType.UNKNOWN ? null
                    : org.basaltmc.bukkit.entity.EntityCore.template(f.world(), type, f.real("x"), f.real("y"),
                        f.real("z"), 0f, 0f);
            }
            return added instanceof Vehicle vehicle ? new org.bukkit.event.vehicle.VehicleCreateEvent(vehicle) : null;
        });
    }
}
