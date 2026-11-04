// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.gameevent;

import com.mojang.serialization.MapCodec;
import io.netty.buffer.ByteBuf;
import java.util.Optional;
import java.util.UUID;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec3;

public class EntityPositionSource implements PositionSource {
    public static final MapCodec<EntityPositionSource> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.gameevent.EntityPositionSource.class, "CODEC");
    public static final StreamCodec<ByteBuf, EntityPositionSource> STREAM_CODEC = StreamCodec.composite(
        ByteBufCodecs.VAR_INT, source -> source.entityId, ByteBufCodecs.FLOAT, source -> source.yOffset, EntityPositionSource::new);
    private final Entity owner;
    private final int entityId;
    private final UUID uuid;
    private final float yOffset;

    public EntityPositionSource(Entity entity, float yOffset) {
        this.owner = entity;
        this.entityId = entity.getId();
        this.uuid = entity.getUUID();
        this.yOffset = yOffset;
    }

    private EntityPositionSource(int entityId, float yOffset) {
        this.owner = null;
        this.entityId = entityId;
        this.uuid = null;
        this.yOffset = yOffset;
    }

    public Optional<Vec3> getPosition(Level level) {
        Entity entity = owner != null && owner.isBound() ? owner : level.getEntity(entityId);
        return entity == null ? Optional.empty() : Optional.of(entity.position().add(0.0, yOffset, 0.0));
    }

    public UUID getUuid() {
        return uuid;
    }

    public PositionSourceType<EntityPositionSource> getType() {
        return PositionSourceType.ENTITY;
    }

    public static class Type implements PositionSourceType<EntityPositionSource> {
        public MapCodec<EntityPositionSource> codec() {
            return CODEC;
        }

        public StreamCodec<ByteBuf, EntityPositionSource> streamCodec() {
            return STREAM_CODEC;
        }
    }
}
