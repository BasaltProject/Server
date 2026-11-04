// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.entity.ContainerOpenersCounter;
public interface ContainerUser {
    default boolean hasContainerOpen(ContainerOpenersCounter container, BlockPos blockPos) {
        return false;
    }
    default double getContainerInteractionRange() {
        return 4.0;
    }
    default LivingEntity getLivingEntity() {
        return this instanceof LivingEntity living ? living : null;
    }
}
