// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.entity;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.world.level.ChunkPos;
import org.basaltmc.bukkit.WorldNatives;
import java.io.IOException;
import java.io.Writer;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.FullChunkStatus;

/** The entities of a level, which the server holds: views are made on request and nothing is kept here. */
public class PersistentEntitySectionManager<T extends EntityAccess> implements AutoCloseable {
    private final ServerLevel level;

    protected PersistentEntitySectionManager() {
        this.level = null;
    }

    public PersistentEntitySectionManager(ServerLevel level) {
        this.level = level;
    }

    public boolean addNewEntity(EntityAccess entity) {
        return ((Entity) entity).spawnIn("custom");
    }

    public List getEntities(ChunkPos chunkPos) {
        String world = level.getWorld().getName();
        List<Entity> found = new ArrayList<>();
        for (int id : WorldNatives.entityIds(world, chunkPos.x(), chunkPos.z(), false)) {
            Entity view = view(id);
            if (view != null) {
                found.add(view);
            }
        }
        return found;
    }

    public LevelEntityGetter<Entity> getEntityGetter() {
        return new LevelEntityGetter<>() {
            @Override
            public EntityAccess get(UUID id) {
                int found = WorldNatives.entityByUuid(level.getWorld().getName(), id.toString());
                return found < 0 ? null : view(found);
            }
        };
    }

    private Entity view(int id) {
        String[] strings = WorldNatives.entityStrings(level.getWorld().getName(), id);
        if (strings == null) {
            return null;
        }
        EntityType<?> type = BuiltInRegistries.ENTITY_TYPE.getValue(Identifier.parse(strings[1]));
        return Entity.view(level, id, UUID.fromString(strings[0]), type);
    }
    public PersistentEntitySectionManager(Class<T> entityClass, LevelCallback<T> callbacks, EntityPersistentStorage<T> permanentStorage) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.PersistentEntitySectionManager#<init>");
    }
    public boolean isTicking(ChunkPos pos) {
        return org.basaltmc.bukkit.WorldNatives.chunkLoaded(level.getWorld().getName(), pos.x(), pos.z());
    }
    public void addLegacyChunkEntities(Stream<T> entities) {
        return;
    }
    public void addWorldGenChunkEntities(Stream<T> entities) {
        return;
    }
    public void updateChunkStatus(ChunkPos pos, FullChunkStatus fullChunkStatus) {
        return;
    }
    public void updateChunkStatus(ChunkPos pos, Visibility chunkStatus) {
        return;
    }
    public void processPendingLoads() {
        return;
    }
    public void tick() {
        return;
    }
    public void autoSave() {
        return;
    }
    public void saveAll() {
        level.getWorld().save();
    }
    public void close() throws IOException {
        return;
    }
    public boolean isLoaded(UUID uuid) {
        return level.getWorld().getEntity(uuid) != null;
    }
    public boolean canPositionTick(BlockPos pos) {
        return org.basaltmc.bukkit.WorldNatives.chunkLoaded(level.getWorld().getName(), pos.getX() >> 4, pos.getZ() >> 4);
    }
    public boolean canPositionTick(ChunkPos pos) {
        return org.basaltmc.bukkit.WorldNatives.chunkLoaded(level.getWorld().getName(), pos.x(), pos.z());
    }
    public boolean areEntitiesLoaded(long chunkKey) {
        return org.basaltmc.bukkit.WorldNatives.chunkLoaded(level.getWorld().getName(), (int) chunkKey, (int) (chunkKey >> 32));
    }
    public void dumpSections(Writer output) throws IOException {
        return;
    }
    public String gatherStats() {
        return "entities: " + count();
    }
    public int count() {
        return level.getWorld().getEntities().size();
    }
    static enum ChunkLoadStatus {
        FRESH, PENDING, LOADED;
    }
    class Callback implements EntityInLevelCallback {
        private Callback(T entity, long currentSectionKey, EntitySection<T> currentSection) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.PersistentEntitySectionManager.Callback#<init>");
        }
        public void onMove() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.PersistentEntitySectionManager.Callback#onMove");
        }
        public void onRemove(Entity.RemovalReason reason) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.PersistentEntitySectionManager.Callback#onRemove");
        }
    }
}
