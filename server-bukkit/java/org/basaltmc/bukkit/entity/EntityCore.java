package org.basaltmc.bukkit.entity;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.UUID;
import org.basaltmc.bukkit.BasaltWorld;
import org.basaltmc.bukkit.BasaltServer;
import org.basaltmc.bukkit.Blocks;
import org.basaltmc.bukkit.EntityNatives;
import org.basaltmc.bukkit.MetadataHolder;
import org.basaltmc.bukkit.WorldNatives;
import org.bukkit.Location;
import org.bukkit.NamespacedKey;
import org.bukkit.World;
import org.bukkit.entity.Entity;
import org.bukkit.entity.EntityType;
import org.bukkit.util.BoundingBox;
import org.bukkit.util.Vector;

public final class EntityCore {
    public static final int kX = 0;
    public static final int kY = 1;
    public static final int kZ = 2;
    public static final int kYaw = 3;
    public static final int kPitch = 4;
    public static final int kVx = 5;
    public static final int kVy = 6;
    public static final int kVz = 7;
    public static final int kHealth = 8;
    public static final int kGround = 9;
    public static final int kAge = 10;
    public static final int kWidth = 11;
    public static final int kHeight = 12;

    private static final Map<String, EntityType> types = new HashMap<>();

    public final BasaltWorld world;
    public volatile int id;
    public volatile UUID uuid;
    public final EntityType type;
    public final MetadataHolder metadata = new MetadataHolder();
    public final java.util.Set<String> tags = new java.util.HashSet<>();

    private volatile double[] staged;
    private Map<String, byte[]> stagedBlobs;
    private Map<String, Object> stagedProperties;
    private List<Object[]> stagedCalls;

    public EntityCore(BasaltWorld world, int id, UUID uuid, EntityType type) {
        this.world = world;
        this.id = id;
        this.uuid = uuid;
        this.type = type;
    }

    /** An entity that does not exist yet: it answers from what it was given and remembers what it is told. */
    public static Entity template(BasaltWorld world, EntityType type, double x, double y, double z, float yaw,
            float pitch) {
        EntityCore core = new EntityCore(world, -1, UUID.randomUUID(), type);
        core.staged = new double[13];
        core.staged[kX] = x;
        core.staged[kY] = y;
        core.staged[kZ] = z;
        core.staged[kYaw] = yaw;
        core.staged[kPitch] = pitch;
        core.stagedProperties = new HashMap<>();
        core.stagedCalls = new ArrayList<>();
        core.stagedBlobs = new HashMap<>();
        return maker().make(type.name(), core);
    }

    public boolean isTemplate() {
        return staged != null;
    }

    /** Turns a template into the entity that now exists, keeping what it was told. */
    public void bind(int realId, UUID realUuid) {
        Map<String, Object> properties = stagedProperties;
        List<Object[]> calls = stagedCalls;
        id = realId;
        uuid = realUuid;
        staged = null;
        stagedProperties = null;
        stagedCalls = null;
        for (Map.Entry<String, Object> entry : properties.entrySet()) {
            set(entry.getKey(), entry.getValue());
        }
        for (Object[] call : calls) {
            call((String) call[0], (Object[]) call[1]);
        }
    }

    public double[] stagedNumbers() {
        return staged;
    }

    /** Hands what a template was told to the entity that took its place. */
    public void replayOnto(EntityCore real) {
        if (staged == null) {
            return;
        }
        for (Map.Entry<String, Object> entry : stagedProperties.entrySet()) {
            real.set(entry.getKey(), entry.getValue());
        }
        for (Object[] call : stagedCalls) {
            real.call((String) call[0], (Object[]) call[1]);
        }
    }

    public static synchronized EntityType typeOf(String key) {
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

    private static EntityMaker maker;

    private static synchronized EntityMaker maker() {
        if (maker == null) {
            try {
                maker = (EntityMaker) Class.forName("org.basaltmc.bukkit.gen.EntityLeaves").getDeclaredConstructor()
                    .newInstance();
            } catch (ReflectiveOperationException failure) {
                throw new IllegalStateException(failure);
            }
        }
        return maker;
    }

    public static Entity make(BasaltWorld world, int id) {
        String[] strings = WorldNatives.entityStrings(world.getName(), id);
        if (strings == null) {
            return null;
        }
        EntityType type = typeOf(strings[1]);
        UUID uuid = UUID.fromString(strings[0]);
        return maker().make(type.name(), new EntityCore(world, id, uuid, type));
    }

    public double[] numbers() {
        return staged != null ? staged : WorldNatives.entityNumbers(world.getName(), id);
    }

    public double number(int index) {
        double[] values = numbers();
        return values == null ? 0.0 : values[index];
    }

    public boolean alive() {
        return staged != null || WorldNatives.entityStrings(world.getName(), id) != null;
    }

    public Location location() {
        double[] values = numbers();
        return values == null ? new Location(world, 0, 0, 0)
            : new Location(world, values[kX], values[kY], values[kZ], (float) values[kYaw], (float) values[kPitch]);
    }

    public Location location(Location into) {
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

    public Vector velocity() {
        double[] values = numbers();
        return values == null ? new Vector() : new Vector(values[kVx], values[kVy], values[kVz]);
    }

    public void velocity(Vector vector) {
        if (staged != null) {
            staged[kVx] = vector.getX();
            staged[kVy] = vector.getY();
            staged[kVz] = vector.getZ();
            return;
        }
        WorldNatives.entityVelocity(world.getName(), id, vector.getX(), vector.getY(), vector.getZ());
    }

    public boolean teleport(Location target) {
        if (staged != null) {
            staged[kX] = target.getX();
            staged[kY] = target.getY();
            staged[kZ] = target.getZ();
            staged[kYaw] = target.getYaw();
            staged[kPitch] = target.getPitch();
            return true;
        }
        if (target.getWorld() != null && !target.getWorld().equals(world)) {
            return false;
        }
        Blocks.flushAll();
        return WorldNatives.entityMove(world.getName(), id, target.getX(), target.getY(), target.getZ(),
            target.getYaw(), target.getPitch());
    }

    public void remove() {
        if (staged != null) {
            return;
        }
        WorldNatives.entityRemove(world.getName(), id);
    }

    public BoundingBox box() {
        double[] values = numbers();
        if (values == null) {
            return new BoundingBox();
        }
        double half = values[kWidth] / 2.0;
        return new BoundingBox(values[kX] - half, values[kY], values[kZ] - half, values[kX] + half,
            values[kY] + values[kHeight], values[kZ] + half);
    }

    public List<Entity> nearby(double x, double y, double z) {
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

    public String customName() {
        if (staged != null) {
            return property("custom_name") instanceof String name ? name : null;
        }
        String[] strings = WorldNatives.entityStrings(world.getName(), id);
        return strings == null || !strings[3].equals("1") ? null : strings[2];
    }

    public org.bukkit.block.BlockFace facing() {
        String kind = type.getKey().getKey();
        if (kind.equals("painting")) {
            return switch (nbtInt("facing") & 3) {
                case 0 -> org.bukkit.block.BlockFace.SOUTH;
                case 1 -> org.bukkit.block.BlockFace.WEST;
                case 2 -> org.bukkit.block.BlockFace.NORTH;
                default -> org.bukkit.block.BlockFace.EAST;
            };
        }
        if (kind.equals("item_frame") || kind.equals("glow_item_frame")) {
            return switch (nbtInt("Facing")) {
                case 0 -> org.bukkit.block.BlockFace.DOWN;
                case 1 -> org.bukkit.block.BlockFace.UP;
                case 2 -> org.bukkit.block.BlockFace.NORTH;
                case 3 -> org.bukkit.block.BlockFace.SOUTH;
                case 4 -> org.bukkit.block.BlockFace.WEST;
                default -> org.bukkit.block.BlockFace.EAST;
            };
        }
        double yaw = ((number(kYaw) % 360) + 360) % 360;
        if (yaw >= 315 || yaw < 45) {
            return org.bukkit.block.BlockFace.SOUTH;
        }
        return yaw < 135 ? org.bukkit.block.BlockFace.WEST
            : yaw < 225 ? org.bukkit.block.BlockFace.NORTH : org.bukkit.block.BlockFace.EAST;
    }

    public String keyName() {
        return type.getKey().getKey().toLowerCase(Locale.ROOT);
    }

    public World worldRef() {
        return world;
    }

    public org.bukkit.persistence.PersistentDataContainer persistentData() {
        return PdcHook.of(this);
    }

    public Object property(String name) {
        return staged != null ? stagedProperties.get(name) : EntityNatives.get(world.getName(), id, name);
    }

    public boolean flag(String name) {
        return property(name) instanceof Boolean value && value;
    }

    public int integer(String name) {
        return property(name) instanceof Number value ? value.intValue() : 0;
    }

    public double real(String name) {
        return property(name) instanceof Number value ? value.doubleValue() : 0.0;
    }

    public String text(String name) {
        return property(name) instanceof String value ? value : null;
    }

    public String[] list(String name) {
        return property(name) instanceof String[] value ? value : new String[0];
    }

    public boolean set(String name, Object value) {
        if (staged != null) {
            stagedProperties.put(name, value);
            return true;
        }
        return EntityNatives.set(world.getName(), id, name, value);
    }

    public Object call(String operation, Object... arguments) {
        if (staged != null) {
            stagedCalls.add(new Object[] {operation, arguments});
            return null;
        }
        return EntityNatives.call(world.getName(), id, operation, arguments);
    }

    public boolean test(String operation, Object... arguments) {
        return call(operation, arguments) instanceof Boolean value && value;
    }

    public Entity entity(int otherId) {
        return otherId < 0 ? null : BasaltServer.instance().entityById(world, otherId);
    }

    public org.bukkit.attribute.AttributeInstance attribute(org.bukkit.attribute.Attribute attribute) {
        return test("has_attribute", attribute.getKey().toString()) ? new BasaltAttributeInstance(this, attribute)
            : null;
    }

    public org.bukkit.inventory.EntityEquipment equipment(Entity holder) {
        return new BasaltEntityEquipment(this, holder);
    }

    public volatile org.bukkit.event.entity.EntityDamageEvent lastDamageCause;

    public Object nbtValue(String path) {
        if (staged != null) {
            return stagedProperties.get("nbt:" + path);
        }
        return call("nbt_value", path);
    }

    public boolean nbtFlag(String path) {
        Object value = nbtValue(path);
        return value instanceof Number number ? number.longValue() != 0 : value instanceof Boolean flag && flag;
    }

    public int nbtInt(String path) {
        return nbtValue(path) instanceof Number value ? value.intValue() : 0;
    }

    public int nbtInt(String path, int fallback) {
        return nbtValue(path) instanceof Number value ? value.intValue() : fallback;
    }

    public double nbtReal(String path) {
        return nbtValue(path) instanceof Number value ? value.doubleValue() : 0.0;
    }

    public String nbtText(String path) {
        return nbtValue(path) instanceof String value ? value : null;
    }

    public void nbtSet(String path, String type, Object value) {
        if (staged != null) {
            stagedProperties.put("nbt:" + path, value);
        }
        call("nbt_set", path, type, value);
    }

    public void nbtRemove(String path) {
        if (staged != null) {
            stagedProperties.remove("nbt:" + path);
        }
        call("nbt_remove", path);
    }

    public byte[] nbt(String path) {
        if (staged != null) {
            return stagedBlobs.get(path);
        }
        return call("nbt", path) instanceof byte[] value ? value : null;
    }

    public void stageNbt(String path, byte[] compound) {
        if (staged != null) {
            stagedBlobs.put(path, compound);
        }
    }

    public void mergeNbt(byte[] patch) {
        if (staged != null) {
            net.minecraft.nbt.CompoundTag parsed = org.basaltmc.bukkit.facade.NbtCodec.fromBytes(patch);
            for (String key : parsed.keySet()) {
                if (parsed.get(key) instanceof net.minecraft.nbt.CompoundTag inner) {
                    stagedBlobs.put(key, org.basaltmc.bukkit.facade.NbtCodec.toBytes(inner));
                }
            }
        }
        call("merge_nbt", patch);
    }

    public int idOf(Entity other) {
        return other == null ? -1 : other.getEntityId();
    }
}
