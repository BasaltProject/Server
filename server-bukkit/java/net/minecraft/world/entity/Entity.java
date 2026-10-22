package net.minecraft.world.entity;

import java.util.UUID;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.TypedInstance;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.DoubleTag;
import net.minecraft.nbt.FloatTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.network.syncher.SyncedDataHolder;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.debug.DebugValueSource;
import net.minecraft.world.Nameable;
import net.minecraft.world.level.entity.EntityAccess;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.scores.ScoreHolder;
import org.basaltmc.bukkit.EntityBridge;
import org.basaltmc.bukkit.Unsupported;
import org.basaltmc.bukkit.WorldNatives;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.basaltmc.bukkit.facade.ServerAccess;
import org.basaltmc.bukkit.facade.ValueIo;
import org.bukkit.craftbukkit.entity.CraftEntity;
import org.bukkit.event.entity.CreatureSpawnEvent;

public class Entity implements Nameable, EntityAccess, ScoreHolder, SyncedDataHolder, DataComponentGetter, ItemOwner,
    SlotProvider, DebugValueSource, TypedInstance {
    public boolean generation = false;
    public CreatureSpawnEvent.SpawnReason spawnReason;

    private ServerLevel level;
    private EntityType<?> type;
    private int id = -1;
    private UUID uuid;
    private CompoundTag pending;
    private CompoundTag snapshot;
    private boolean removed;

    protected Entity() {
    }

    public Entity(EntityType<?> type, ServerLevel level) {
        this.type = type;
        this.level = level;
        this.pending = new CompoundTag();
        this.pending.putString("id", EntityType.getKey(type).toString());
    }

    public static Entity view(ServerLevel level, int id, UUID uuid, EntityType<?> type) {
        Entity made = EntityBridge.isAlive(EntityType.getKey(type).toString()) ? new LivingEntity() : new Entity();
        made.level = level;
        made.id = id;
        made.uuid = uuid;
        made.type = type;
        return made;
    }

    public int getId() {
        return id;
    }

    public boolean isBound() {
        return id >= 0;
    }

    public ServerLevel level() {
        return level;
    }

    private String world() {
        return level.getWorld().getName();
    }

    public void absSnapTo(double x, double y, double z, float yRot, float xRot) {
        if (pending == null) {
            WorldNatives.entityMove(world(), id, x, y, z, yRot, xRot);
            return;
        }
        ListTag pos = new ListTag();
        pos.add(DoubleTag.valueOf(x));
        pos.add(DoubleTag.valueOf(y));
        pos.add(DoubleTag.valueOf(z));
        pending.put("Pos", pos);
        ListTag rotation = new ListTag();
        rotation.add(FloatTag.valueOf(yRot));
        rotation.add(FloatTag.valueOf(xRot));
        pending.put("Rotation", rotation);
    }

    public void discard() {
        if (pending != null) {
            removed = true;
            return;
        }
        snapshot = nbt();
        removed = true;
        WorldNatives.entityRemove(world(), id);
    }

    public boolean isRemoved() {
        return removed || (pending == null && WorldNatives.entityStrings(world(), id) == null);
    }

    public CraftEntity getBukkitEntity() {
        if (pending != null) {
            throw Unsupported.hit("net.minecraft.world.entity.Entity.getBukkitEntity");
        }
        return (CraftEntity) EntityBridge.of(level.getWorld(), id);
    }

    public Vec3 getPosition(float partialTickTime) {
        throw Unsupported.hit("net.minecraft.world.entity.Entity.getPosition");
    }

    public String getStringUUID() {
        return getUUID().toString();
    }

    public EntityType getType() {
        return type;
    }

    public UUID getUUID() {
        if (uuid == null) {
            int[] parts = pending == null ? null : pending.getIntArray("UUID").orElse(null);
            uuid = parts != null && parts.length == 4
                ? new UUID((long) parts[0] << 32 | (parts[1] & 0xFFFFFFFFL), (long) parts[2] << 32 | (parts[3] & 0xFFFFFFFFL))
                : UUID.randomUUID();
        }
        return uuid;
    }

    public void setUUID(UUID value) {
        uuid = value;
        if (pending != null) {
            pending.putIntArray("UUID", new int[] {(int) (value.getMostSignificantBits() >> 32),
                (int) value.getMostSignificantBits(), (int) (value.getLeastSignificantBits() >> 32),
                (int) value.getLeastSignificantBits()});
        }
    }

    public void load(ValueInput input) {
        loadTag(ValueIo.read(input));
    }

    public void loadTag(CompoundTag tag) {
        if (pending == null) {
            throw Unsupported.hit("net.minecraft.world.entity.Entity.load");
        }
        CompoundTag data = tag.copy();
        data.remove("id");
        pending.merge(data);
        uuid = null;
    }

    public RegistryAccess registryAccess() {
        return ServerAccess.registries();
    }

    public boolean save(ValueOutput output) {
        CompoundTag tag = nbt();
        if (tag == null) {
            return false;
        }
        ValueIo.write(output, tag);
        return true;
    }

    public void saveWithoutId(ValueOutput output) {
        CompoundTag tag = nbt();
        if (tag != null) {
            tag.remove("id");
            ValueIo.write(output, tag);
        }
    }

    public CompoundTag nbt() {
        if (pending != null) {
            return pending.copy();
        }
        if (!removed) {
            CompoundTag live = NbtCodec.fromBytes(WorldNatives.entityNbt(world(), id));
            if (live != null) {
                return live;
            }
        }
        return snapshot == null ? null : snapshot.copy();
    }

    public boolean spawnIn(String cause) {
        if (pending == null) {
            return true;
        }
        int made = WorldNatives.spawnEntityNbt(world(), NbtCodec.toBytes(pending), cause);
        if (made < 0) {
            return false;
        }
        id = made;
        snapshot = pending;
        pending = null;
        return true;
    }

    public static Identifier keyOf(Entity entity) {
        return EntityType.getKey(entity.type);
    }
}
