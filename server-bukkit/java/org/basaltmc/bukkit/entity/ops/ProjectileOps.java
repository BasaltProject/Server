package org.basaltmc.bukkit.entity.ops;

import java.util.UUID;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Projectile;
import org.bukkit.projectiles.ProjectileSource;

public interface ProjectileOps extends Projectile, Cored {
    default ProjectileSource getShooter() {
        return core().entity(core().integer("owner_id")) instanceof ProjectileSource source ? source : null;
    }

    default void setShooter(ProjectileSource source) {
        core().set("owner_id", source instanceof Entity entity ? entity.getEntityId() : -1);
    }

    default UUID getOwnerUniqueId() {
        return getShooter() instanceof Entity entity ? entity.getUniqueId() : null;
    }

    default boolean hasBeenShot() {
        return core().nbtFlag("HasBeenShot");
    }

    default void setHasBeenShot(boolean shot) {
        core().nbtSet("HasBeenShot", "bool", shot);
    }

    default boolean hasLeftShooter() {
        return core().nbtFlag("LeftOwner");
    }

    default void setHasLeftShooter(boolean left) {
        core().nbtSet("LeftOwner", "bool", left);
    }

    default boolean canHitEntity(Entity entity) {
        if (entity == null || !entity.isValid()) {
            return false;
        }
        if (entity instanceof org.bukkit.entity.Player player
            && player.getGameMode() == org.bukkit.GameMode.SPECTATOR) {
            return false;
        }
        return !(entity.equals(getShooter()) && !hasLeftShooter());
    }
}
