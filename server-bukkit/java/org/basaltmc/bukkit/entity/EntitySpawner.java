package org.basaltmc.bukkit.entity;

import java.util.UUID;
import java.util.function.Consumer;
import org.basaltmc.bukkit.BasaltWorld;
import org.basaltmc.bukkit.Blocks;
import org.basaltmc.bukkit.EntityNatives;
import org.bukkit.Location;
import org.bukkit.entity.Entity;
import org.bukkit.entity.EntityType;
import org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason;

public final class EntitySpawner {
    private EntitySpawner() {}

    public static EntityType typeOf(Class<? extends Entity> type) {
        for (EntityType candidate : EntityType.values()) {
            if (candidate != EntityType.UNKNOWN && candidate.getEntityClass() == type) {
                return candidate;
            }
        }
        throw new IllegalArgumentException("Cannot spawn an entity for " + type.getName());
    }

    public static Entity spawn(BasaltWorld world, Location at, EntityType type, SpawnReason reason,
            Consumer<? super Entity> before) {
        if (type == EntityType.UNKNOWN || type == EntityType.PLAYER || type.getEntityClass() == null) {
            throw new IllegalArgumentException("Cannot spawn an entity of type " + type);
        }
        Blocks.flushAll();
        int spawned = EntityNatives.spawnAs(world.getName(), type.getKey().toString(), at.getX(), at.getY(), at.getZ(),
            at.getYaw(), reason == null ? "custom" : reason.name().toLowerCase(java.util.Locale.ROOT));
        Entity made = spawned < 0 ? null : world.entity(spawned);
        if (made != null && before != null) {
            before.accept(made);
        }
        return made;
    }

    @SuppressWarnings("unchecked")
    public static <T extends Entity> T spawn(BasaltWorld world, Location at, Class<T> type, SpawnReason reason,
            Consumer<? super T> before) {
        return (T) spawn(world, at, typeOf(type), reason, before == null ? null : entity -> before.accept((T) entity));
    }

    public static org.bukkit.entity.Item drop(BasaltWorld world, Location at, org.bukkit.inventory.ItemStack stack,
            double vx, double vy, double vz, Consumer<? super org.bukkit.entity.Item> before) {
        byte[] nbt = EntityItems.toNbt(stack);
        if (nbt == null) {
            throw new IllegalArgumentException("Cannot drop an empty item stack");
        }
        Blocks.flushAll();
        int id = EntityNatives.dropItem(world.getName(), nbt, at.getX(), at.getY(), at.getZ(), vx, vy, vz);
        Entity made = id < 0 ? null : world.entity(id);
        if (made instanceof org.bukkit.entity.Item item) {
            if (before != null) {
                before.accept(item);
            }
            return item;
        }
        return null;
    }

    @SuppressWarnings("unchecked")
    public static <T extends Entity> T create(BasaltWorld world, Location at, Class<T> type) {
        EntityType kind = typeOf(type);
        if (kind == EntityType.PLAYER) {
            throw new IllegalArgumentException("Cannot create a player");
        }
        return (T) EntityCore.template(world, kind, at.getX(), at.getY(), at.getZ(), at.getYaw(), at.getPitch());
    }

    @SuppressWarnings("unchecked")
    public static <T extends Entity> T add(BasaltWorld world, T entity, SpawnReason reason) {
        if (!(entity instanceof Cored cored) || !cored.core().isTemplate()) {
            throw new IllegalArgumentException("Entity has already been added to a world");
        }
        EntityCore core = cored.core();
        double[] at = core.stagedNumbers();
        Blocks.flushAll();
        int id = EntityNatives.spawnAs(world.getName(), core.type.getKey().toString(), at[EntityCore.kX],
            at[EntityCore.kY], at[EntityCore.kZ], (float) at[EntityCore.kYaw],
            reason == null ? "custom" : reason.name().toLowerCase(java.util.Locale.ROOT));
        if (id < 0) {
            return null;
        }
        String[] strings = org.basaltmc.bukkit.WorldNatives.entityStrings(world.getName(), id);
        core.bind(id, UUID.fromString(strings[0]));
        world.adopt(id, entity);
        if (entity instanceof org.bukkit.entity.ExperienceOrb && core.nbtText("SpawnReason") == null) {
            core.nbtSet("SpawnReason", "string", "CUSTOM");
        }
        return entity;
    }
}
