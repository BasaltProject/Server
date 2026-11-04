// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import net.minecraft.world.level.Level;

public interface OwnableEntity {
    EntityReference<LivingEntity> getOwnerReference();

    Level level();

    default LivingEntity getOwner() {
        return EntityReference.getLivingEntity(getOwnerReference(), level());
    }

    default LivingEntity getRootOwner() {
        LivingEntity current = getOwner();
        while (current instanceof OwnableEntity ownable && ownable.getOwner() != null) {
            current = ownable.getOwner();
        }
        return current;
    }
}
