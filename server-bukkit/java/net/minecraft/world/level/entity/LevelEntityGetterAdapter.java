// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.entity;
import java.util.UUID;
import java.util.function.Consumer;
import net.minecraft.util.AbortableIterationConsumer;
import net.minecraft.world.phys.AABB;
public class LevelEntityGetterAdapter<T extends EntityAccess> implements LevelEntityGetter<T> {
    private final EntityLookup<T> visibleEntities;
    private final EntitySectionStorage<T> sectionStorage;
    public LevelEntityGetterAdapter(EntityLookup<T> visibleEntities, EntitySectionStorage<T> sectionStorage) {
        this.visibleEntities = visibleEntities;
        this.sectionStorage = sectionStorage;
    }
    public T get(int id) {
        return visibleEntities.getEntity(id);
    }
    public T get(UUID id) {
        return visibleEntities.getEntity(id);
    }
    public Iterable<T> getAll() {
        return visibleEntities.getAllEntities();
    }
    public <U extends T> void get(EntityTypeTest<T, U> type, AbortableIterationConsumer<U> consumer) {
        visibleEntities.getEntities(type, consumer);
    }
    public void get(AABB bb, Consumer<T> output) {
        sectionStorage.getEntities(bb, AbortableIterationConsumer.forConsumer(output));
    }
    public <U extends T> void get(EntityTypeTest<T, U> type, AABB bb, AbortableIterationConsumer<U> consumer) {
        sectionStorage.getEntities(type, bb, consumer);
    }
}
