// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.entity;
import java.util.stream.Stream;
import net.minecraft.util.AbortableIterationConsumer;
import net.minecraft.util.AbortableIterationConsumer.Continuation;
import net.minecraft.world.phys.AABB;
public class EntitySection<T extends EntityAccess> {
    private final java.util.List<T> storage = new java.util.ArrayList<>();
    private Visibility chunkStatus;
    public EntitySection(Class<T> entityClass, Visibility chunkStatus) {
        this.chunkStatus = chunkStatus;
    }
    public void add(T entity) {
        storage.add(entity);
    }
    public boolean remove(T entity) {
        return storage.remove(entity);
    }
    public Continuation getEntities(AABB bb, AbortableIterationConsumer<T> entities) {
        for (T each : storage) {
            if (each.getBoundingBox().intersects(bb) && entities.accept(each).shouldAbort()) {
                return Continuation.ABORT;
            }
        }
        return Continuation.CONTINUE;
    }
    public <U extends T> Continuation getEntities(EntityTypeTest<T, U> type, AABB bb, AbortableIterationConsumer<? super U> consumer) {
        for (T each : storage) {
            if (!type.getBaseClass().isInstance(each)) {
                continue;
            }
            U match = (U) type.tryCast(each);
            if (match != null && each.getBoundingBox().intersects(bb) && consumer.accept(match).shouldAbort()) {
                return Continuation.ABORT;
            }
        }
        return Continuation.CONTINUE;
    }
    public boolean isEmpty() {
        return storage.isEmpty();
    }
    public Stream<T> getEntities() {
        return storage.stream();
    }
    public Visibility getStatus() {
        return chunkStatus;
    }
    public Visibility updateChunkStatus(Visibility chunkStatus) {
        Visibility before = chunkStatus;
        this.chunkStatus = chunkStatus;
        return before;
    }
    public int size() {
        return storage.size();

    }
}
