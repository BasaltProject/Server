// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.entity;
import java.util.Objects;
import it.unimi.dsi.fastutil.longs.LongOpenHashSet;
import net.minecraft.core.SectionPos;
import net.minecraft.world.level.ChunkPos;
import it.unimi.dsi.fastutil.longs.Long2ObjectFunction;
import it.unimi.dsi.fastutil.longs.LongSet;
import java.util.stream.LongStream;
import java.util.stream.Stream;
import net.minecraft.util.AbortableIterationConsumer;
import net.minecraft.world.phys.AABB;
public class EntitySectionStorage<T extends EntityAccess> {
    private final Class<T> entityClass;
    private final Long2ObjectFunction<Visibility> intialSectionVisibility;
    private final it.unimi.dsi.fastutil.longs.Long2ObjectMap<EntitySection<T>> sections = new it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap<>();
    private final it.unimi.dsi.fastutil.longs.LongSortedSet sectionIds = new it.unimi.dsi.fastutil.longs.LongAVLTreeSet();
    public static final int CHONKY_ENTITY_SEARCH_GRACE = 2;
    public static final int MAX_NON_CHONKY_ENTITY_SIZE = 4;
    public EntitySectionStorage(Class<T> entityClass, Long2ObjectFunction<Visibility> intialSectionVisibility) {
        this.entityClass = entityClass;
        this.intialSectionVisibility = intialSectionVisibility;
    }
    public void forEachAccessibleNonEmptySection(AABB bb, AbortableIterationConsumer<EntitySection<T>> output) {
        int fromX = SectionPos.posToSectionCoord(bb.minX - 2.0);
        int fromY = SectionPos.posToSectionCoord(bb.minY - 4.0);
        int fromZ = SectionPos.posToSectionCoord(bb.minZ - 2.0);
        int toX = SectionPos.posToSectionCoord(bb.maxX + 2.0);
        int toY = SectionPos.posToSectionCoord(bb.maxY);
        int toZ = SectionPos.posToSectionCoord(bb.maxZ + 2.0);
        for (int cx = fromX; cx <= toX; cx++) {
            var column = sectionIds.subSet(SectionPos.asLong(cx, 0, 0), SectionPos.asLong(cx, -1, -1) + 1L);
            for (long key : column) {
                int sy = SectionPos.y(key);
                int sz = SectionPos.z(key);
                if (sy < fromY || sy > toY || sz < fromZ || sz > toZ) {
                    continue;
                }
                EntitySection<T> found = sections.get(key);
                if (found != null && !found.isEmpty() && found.getStatus().isAccessible() && output.accept(found).shouldAbort()) {
                    return;
                }
            }
        }
    }
    public LongStream getExistingSectionPositionsInChunk(long chunkKey) {
        var chunkSections = sectionsOfChunk(ChunkPos.getX(chunkKey), ChunkPos.getZ(chunkKey));
        return chunkSections.isEmpty() ? LongStream.empty() : chunkSections.longStream();
    }
    public Stream<EntitySection<T>> getExistingSectionsInChunk(long chunkKey) {
        return getExistingSectionPositionsInChunk(chunkKey).mapToObj(sections::get).filter(Objects::nonNull);
    }
    public EntitySection<T> getOrCreateSection(long key) {
        EntitySection<T> existing = sections.get(key);
        if (existing == null) {
            existing = new EntitySection<>(entityClass, intialSectionVisibility.get(key));
            sections.put(key, existing);
            sectionIds.add(key);
        }
        return existing;
    }
    public EntitySection<T> getSection(long key) {
        return sections.get(key);
    }
    public LongSet getAllChunksWithExistingSections() {
        LongSet chunks = new LongOpenHashSet();
        for (long sectionKey : sections.keySet()) {
            chunks.add(ChunkPos.pack(SectionPos.x(sectionKey), SectionPos.z(sectionKey)));
        }
        return chunks;
    }
    public void getEntities(AABB bb, AbortableIterationConsumer<T> output) {
        forEachAccessibleNonEmptySection(bb, section -> section.getEntities(bb, output));
    }
    public <U extends T> void getEntities(EntityTypeTest<T, U> type, AABB bb, AbortableIterationConsumer<U> consumer) {
        forEachAccessibleNonEmptySection(bb, section -> section.getEntities(type, bb, consumer));
    }
    public void remove(long sectionKey) {
        sections.remove(sectionKey);
        sectionIds.remove(sectionKey);
    }
    private it.unimi.dsi.fastutil.longs.LongSortedSet sectionsOfChunk(int chunkX, int chunkZ) {
        it.unimi.dsi.fastutil.longs.LongSortedSet hits = new it.unimi.dsi.fastutil.longs.LongAVLTreeSet();
        for (long key : sectionIds) {
            if (SectionPos.x(key) == chunkX && SectionPos.z(key) == chunkZ) {
                hits.add(key);
            }
        }
        return hits;
    }
    public int count() {
        return sectionIds.size();
    }
}
