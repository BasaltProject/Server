package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.EntityCore;
import org.bukkit.Location;

final class MobPositions {
    private MobPositions() {}

    static Location parse(EntityCore core, String text) {
        if (text == null || text.isEmpty()) {
            return null;
        }
        String[] parts = text.split(",");
        return new Location(core.worldRef(), Integer.parseInt(parts[0]), Integer.parseInt(parts[1]),
            Integer.parseInt(parts[2]));
    }

    static String format(Location location) {
        return location.getBlockX() + "," + location.getBlockY() + "," + location.getBlockZ();
    }
}
