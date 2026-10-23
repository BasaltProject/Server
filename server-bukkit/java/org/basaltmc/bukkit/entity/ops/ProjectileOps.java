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
}
