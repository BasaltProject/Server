package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.UUID;
import org.bukkit.Location;
import org.bukkit.NamespacedKey;
import org.bukkit.World;
import org.bukkit.entity.Entity;
import org.bukkit.entity.EntityType;
import org.bukkit.util.BoundingBox;
import org.bukkit.util.Vector;

final class EntityCore {
    static final int kX = 0;
    static final int kY = 1;
    static final int kZ = 2;
    static final int kYaw = 3;
    static final int kPitch = 4;
    static final int kVx = 5;
    static final int kVy = 6;
    static final int kVz = 7;
    static final int kHealth = 8;
    static final int kGround = 9;
    static final int kAge = 10;
    static final int kWidth = 11;
    static final int kHeight = 12;

    private static final Map<String, EntityType> types = new HashMap<>();

    final BasaltWorld world;
    final int id;
    final UUID uuid;
    final EntityType type;
    final MetadataHolder metadata = new MetadataHolder();
    final java.util.Set<String> tags = new java.util.HashSet<>();

    EntityCore(BasaltWorld world, int id, UUID uuid, EntityType type) {
        this.world = world;
        this.id = id;
        this.uuid = uuid;
        this.type = type;
    }

    static synchronized EntityType typeOf(String key) {
        EntityType found = types.get(key);
        if (found == null) {
            NamespacedKey parsed = NamespacedKey.fromString(key);
            for (EntityType candidate : EntityType.values()) {
                if (candidate != EntityType.UNKNOWN && candidate.getKey().equals(parsed)) {
                    found = candidate;
                    break;
                }
            }
            if (found == null) {
                found = EntityType.UNKNOWN;
            }
            types.put(key, found);
        }
        return found;
    }

    static Entity make(BasaltWorld world, int id) {
        String[] strings = WorldNatives.entityStrings(world.getName(), id);
        if (strings == null) {
            return null;
        }
        EntityType type = typeOf(strings[1]);
        UUID uuid = UUID.fromString(strings[0]);
        return type.isAlive() ? new BasaltLivingEntity(new EntityCore(world, id, uuid, type))
            : new BasaltEntity(new EntityCore(world, id, uuid, type));
    }

    double[] numbers() {
        return WorldNatives.entityNumbers(world.getName(), id);
    }

    double number(int index) {
        double[] values = numbers();
        return values == null ? 0.0 : values[index];
    }

    boolean alive() {
        return WorldNatives.entityStrings(world.getName(), id) != null;
    }

    Location location() {
        double[] values = numbers();
        return values == null ? new Location(world, 0, 0, 0)
            : new Location(world, values[kX], values[kY], values[kZ], (float) values[kYaw], (float) values[kPitch]);
    }

    Location location(Location into) {
        if (into != null) {
            Location here = location();
            into.setWorld(world);
            into.setX(here.getX());
            into.setY(here.getY());
            into.setZ(here.getZ());
            into.setYaw(here.getYaw());
            into.setPitch(here.getPitch());
        }
        return into;
    }

    Vector velocity() {
        double[] values = numbers();
        return values == null ? new Vector() : new Vector(values[kVx], values[kVy], values[kVz]);
    }

    void velocity(Vector vector) {
        WorldNatives.entityVelocity(world.getName(), id, vector.getX(), vector.getY(), vector.getZ());
    }

    boolean teleport(Location target) {
        if (target.getWorld() != null && !target.getWorld().equals(world)) {
            return false;
        }
        Blocks.flushAll();
        return WorldNatives.entityMove(world.getName(), id, target.getX(), target.getY(), target.getZ(),
            target.getYaw(), target.getPitch());
    }

    void remove() {
        WorldNatives.entityRemove(world.getName(), id);
    }

    BoundingBox box() {
        double[] values = numbers();
        if (values == null) {
            return new BoundingBox();
        }
        double half = values[kWidth] / 2.0;
        return new BoundingBox(values[kX] - half, values[kY], values[kZ] - half, values[kX] + half,
            values[kY] + values[kHeight], values[kZ] + half);
    }

    List<Entity> nearby(double x, double y, double z) {
        Location here = location();
        List<Entity> out = new ArrayList<>();
        for (Entity other : world.getEntities()) {
            if (other.getEntityId() == id) {
                continue;
            }
            Location there = other.getLocation();
            if (Math.abs(there.getX() - here.getX()) <= x && Math.abs(there.getY() - here.getY()) <= y
                && Math.abs(there.getZ() - here.getZ()) <= z) {
                out.add(other);
            }
        }
        return out;
    }

    String customName() {
        String[] strings = WorldNatives.entityStrings(world.getName(), id);
        return strings == null || !strings[3].equals("1") ? null : strings[2];
    }

    org.bukkit.block.BlockFace facing() {
        double yaw = ((number(kYaw) % 360) + 360) % 360;
        if (yaw >= 315 || yaw < 45) {
            return org.bukkit.block.BlockFace.SOUTH;
        }
        return yaw < 135 ? org.bukkit.block.BlockFace.WEST
            : yaw < 225 ? org.bukkit.block.BlockFace.NORTH : org.bukkit.block.BlockFace.EAST;
    }

    String keyName() {
        return type.getKey().getKey().toLowerCase(Locale.ROOT);
    }

    World worldRef() {
        return world;
    }
}
