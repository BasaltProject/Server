package org.basaltmc.bukkit.facade;

import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.Vec3;

/** The first non-spectator entity on a segment from a player's eye, by the nearest ray hit. */
public final class EntityRays {
    private EntityRays() {}

    public static Entity firstEntity(ServerPlayer player, Vec3 from, Vec3 to) {
        org.bukkit.entity.Player bukkit = player.bukkitPlayer();
        if (bukkit == null) {
            return null;
        }
        Vec3 direction = to.subtract(from);
        double distance = direction.length();
        org.bukkit.util.RayTraceResult hit = bukkit.getWorld().rayTraceEntities(bukkit.getEyeLocation(),
            new org.bukkit.util.Vector(direction.x, direction.y, direction.z).normalize(), distance, 0.0,
            other -> other.getEntityId() != bukkit.getEntityId() && !(other instanceof org.bukkit.entity.Player p && p.getGameMode() == org.bukkit.GameMode.SPECTATOR));
        return hit == null || hit.getHitEntity() == null ? null : player.level().getEntity(hit.getHitEntity().getEntityId());
    }
}
