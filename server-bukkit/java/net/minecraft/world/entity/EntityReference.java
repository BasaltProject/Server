// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.UUID;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.entity.UUIDLookup;
import net.minecraft.world.level.entity.UniquelyIdentifyable;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;

public final class EntityReference<StoredEntityType extends UniquelyIdentifyable> {
    private static final Codec<UUID> UUID_CODEC = Codec.INT_STREAM.comapFlatMap(stream -> {
        int[] parts = stream.toArray();
        return parts.length == 4 ? com.mojang.serialization.DataResult.success(new UUID((long) parts[0] << 32 | (parts[1] & 0xFFFFFFFFL),
            (long) parts[2] << 32 | (parts[3] & 0xFFFFFFFFL))) : com.mojang.serialization.DataResult.error(() -> "Expected 4 ints for a UUID");
    }, uuid -> java.util.stream.IntStream.of((int) (uuid.getMostSignificantBits() >> 32), (int) uuid.getMostSignificantBits(),
        (int) (uuid.getLeastSignificantBits() >> 32), (int) uuid.getLeastSignificantBits()));

    private StoredEntityType entity;
    private final UUID uuid;

    public static <Type extends UniquelyIdentifyable> Codec<EntityReference<Type>> codec() {
        return UUID_CODEC.xmap(EntityReference::of, EntityReference::getUUID);
    }

    public static <Type extends UniquelyIdentifyable> StreamCodec<ByteBuf, EntityReference<Type>> streamCodec() {
        return StreamCodec.of((buf, ref) -> {
            buf.writeLong(ref.uuid.getMostSignificantBits());
            buf.writeLong(ref.uuid.getLeastSignificantBits());
        }, buf -> EntityReference.of(new UUID(buf.readLong(), buf.readLong())));
    }

    private EntityReference(StoredEntityType entity) {
        this.entity = entity;
        this.uuid = entity.getUUID();
    }

    private EntityReference(UUID uuid) {
        this.uuid = uuid;
    }

    public static <T extends UniquelyIdentifyable> EntityReference<T> of(T entity) {
        return new EntityReference<>(entity);
    }

    public static <T extends UniquelyIdentifyable> EntityReference<T> of(UUID uuid) {
        return new EntityReference<>(uuid);
    }

    public UUID getUUID() {
        return uuid;
    }

    @SuppressWarnings("unchecked")
    public StoredEntityType getEntity(UUIDLookup<? extends UniquelyIdentifyable> lookup, Class<StoredEntityType> clazz) {
        if (entity != null && entity.isRemoved()) {
            entity = null;
        }
        if (entity != null) {
            return clazz.isInstance(entity) ? entity : null;
        }
        UniquelyIdentifyable found = lookup.lookup(uuid);
        if (found != null && !found.isRemoved() && clazz.isInstance(found)) {
            entity = (StoredEntityType) found;
            return entity;
        }
        return null;
    }

    public StoredEntityType getEntity(Level level, Class<StoredEntityType> clazz) {
        return Player.class.isAssignableFrom(clazz) ? getEntity(level::getPlayerByUUID, clazz) : getEntity(level::getEntity, clazz);
    }

    public boolean matches(StoredEntityType entity) {
        return uuid.equals(entity.getUUID());
    }

    public void store(ValueOutput output, String key) {
        output.store(key, UUID_CODEC, uuid);
    }

    public static void store(EntityReference<?> reference, ValueOutput output, String key) {
        if (reference != null) {
            reference.store(output, key);
        }
    }

    public static <StoredEntityType extends UniquelyIdentifyable> StoredEntityType get(EntityReference<StoredEntityType> reference, Level level,
        Class<StoredEntityType> clazz) {
        return reference != null ? reference.getEntity(level, clazz) : null;
    }

    public static Entity getEntity(EntityReference<Entity> reference, Level level) {
        return get(reference, level, Entity.class);
    }

    public static LivingEntity getLivingEntity(EntityReference<LivingEntity> reference, Level level) {
        return get(reference, level, LivingEntity.class);
    }

    public static Player getPlayer(EntityReference<Player> reference, Level level) {
        return get(reference, level, Player.class);
    }

    public static <StoredEntityType extends UniquelyIdentifyable> EntityReference<StoredEntityType> read(ValueInput input, String key) {
        return input.read(key, UUID_CODEC).<EntityReference<StoredEntityType>>map(EntityReference::of).orElse(null);
    }

    public static <StoredEntityType extends UniquelyIdentifyable> EntityReference<StoredEntityType> readWithOldOwnerConversion(ValueInput input,
        String key, Level level) {
        return read(input, key);
    }

    @Override
    public boolean equals(Object obj) {
        return obj == this || (obj instanceof EntityReference<?> other && uuid.equals(other.uuid));
    }

    @Override
    public int hashCode() {
        return uuid.hashCode();
    }
}
