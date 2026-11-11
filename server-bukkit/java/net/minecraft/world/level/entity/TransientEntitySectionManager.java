// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.entity;
import it.unimi.dsi.fastutil.longs.LongSet;
import it.unimi.dsi.fastutil.longs.LongOpenHashSet;
import net.minecraft.core.SectionPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.ChunkPos;
public class TransientEntitySectionManager<T extends EntityAccess> {
    private final LevelCallback<T> callbacks;
    private final EntityLookup<T> entityStorage = new EntityLookup<>();
    private final EntitySectionStorage<T> sectionStorage;
    private final LongSet tickingChunks = new LongOpenHashSet();
    private final LevelEntityGetter<T> entityGetter;
    public TransientEntitySectionManager(Class<T> entityClass, LevelCallback<T> callbacks) {
        this.sectionStorage = new EntitySectionStorage<>(entityClass, key -> tickingChunks.contains(key) ? Visibility.TICKING : Visibility.TRACKED);
        this.callbacks = callbacks;
        this.entityGetter = new LevelEntityGetterAdapter<>(entityStorage, sectionStorage);
    }
    public void startTicking(ChunkPos pos) {
        long chunkKey = pos.pack();
        tickingChunks.add(chunkKey);
        sectionStorage.getExistingSectionsInChunk(chunkKey).forEach(section -> {
            if (!section.updateChunkStatus(Visibility.TICKING).isTicking()) {
                section.getEntities().filter(each -> !each.isAlwaysTicking()).forEach(callbacks::onTickingStart);
            }
        });
    }
    public void stopTicking(ChunkPos pos) {
        long chunkKey = pos.pack();
        tickingChunks.remove(chunkKey);
        sectionStorage.getExistingSectionsInChunk(chunkKey).forEach(section -> {
            if (section.updateChunkStatus(Visibility.TRACKED).isTicking()) {
                section.getEntities().filter(each -> !each.isAlwaysTicking()).forEach(callbacks::onTickingEnd);
            }
        });
    }
    public LevelEntityGetter<T> getEntityGetter() {
        return entityGetter;
    }
    public void addEntity(T entity) {
        entityStorage.add(entity);
        long key = SectionPos.asLong(entity.blockPosition());
        EntitySection<T> target = sectionStorage.getOrCreateSection(key);
        target.add(entity);
        entity.setLevelCallback(new Callback(entity, key, target));
        callbacks.onCreated(entity);
        callbacks.onTrackingStart(entity);
        if (entity.isAlwaysTicking() || target.getStatus().isTicking()) {
            callbacks.onTickingStart(entity);
        }
    }
    public int count() {
        return entityStorage.count();
    }
    public String gatherStats() {
        return entityStorage.count() + "," + sectionStorage.count() + "," + tickingChunks.size();
    }
    private void dropIfEmpty(long key, EntitySection<T> section) {
        if (section.isEmpty()) {
            sectionStorage.remove(key);
        }
    }
    class Callback implements EntityInLevelCallback {
        private final T entity;
        private long currentSectionKey;
        private EntitySection<T> currentSection;
        private Callback(T entity, long currentSectionKey, EntitySection<T> currentSection) {
            this.entity = entity;
            this.currentSectionKey = currentSectionKey;
            this.currentSection = currentSection;
        }
        public void onMove() {
            long newKey = SectionPos.asLong(entity.blockPosition());
            if (newKey == currentSectionKey) {
                return;
            }
            Visibility before = currentSection.getStatus();
            currentSection.remove(entity);
            dropIfEmpty(currentSectionKey, currentSection);
            EntitySection<T> next = sectionStorage.getOrCreateSection(newKey);
            next.add(entity);
            currentSection = next;
            currentSectionKey = newKey;
            callbacks.onSectionChange(entity);
            if (!entity.isAlwaysTicking()) {
                boolean was = before.isTicking();
                boolean now = next.getStatus().isTicking();
                if (was && !now) {
                    callbacks.onTickingEnd(entity);
                } else if (!was && now) {
                    callbacks.onTickingStart(entity);
                }
            }
        }
        public void onRemove(Entity.RemovalReason reason) {
            currentSection.remove(entity);
            if (currentSection.getStatus().isTicking() || entity.isAlwaysTicking()) {
                callbacks.onTickingEnd(entity);
            }
            callbacks.onTrackingEnd(entity);
            callbacks.onDestroyed(entity);
            entityStorage.remove(entity);
            entity.setLevelCallback(EntityInLevelCallback.NULL);
            dropIfEmpty(currentSectionKey, currentSection);
        }
    }
}
