package org.basaltmc.bukkit;

import org.bukkit.World;
import org.bukkit.entity.Entity;

public final class EntityBridge {
    private EntityBridge() {}

    public static Entity of(World world, int entityId) {
        return world instanceof BasaltWorld basalt ? basalt.entity(entityId) : null;
    }

    public static net.minecraft.world.entity.Entity handle(World world, int entityId, java.util.UUID uuid, String typeKey) {
        net.minecraft.server.level.ServerLevel level = ((org.bukkit.craftbukkit.CraftWorld) world).getHandle();
        net.minecraft.world.entity.EntityType<?> type = net.minecraft.core.registries.BuiltInRegistries.ENTITY_TYPE
            .getValue(net.minecraft.resources.Identifier.parse(typeKey));
        return net.minecraft.world.entity.Entity.view(level, entityId, uuid, type);
    }

    public static boolean isAlive(String typeKey) {
        return EntityCore.typeOf(typeKey).isAlive();
    }
}
