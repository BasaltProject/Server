// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.entity;
import java.util.UUID;
import net.minecraft.util.AbortableIterationConsumer;
public class EntityLookup<T extends EntityAccess> {
    private final it.unimi.dsi.fastutil.ints.Int2ObjectMap<T> byId = new it.unimi.dsi.fastutil.ints.Int2ObjectLinkedOpenHashMap<>();
    private final java.util.Map<UUID, T> byUuid = new java.util.HashMap<>();
    public <U extends T> void getEntities(EntityTypeTest<T, U> type, AbortableIterationConsumer<U> consumer) {
        for (T candidate : byId.values()) {
            U match = (U) type.tryCast(candidate);
            if (match != null && consumer.accept(match).shouldAbort()) {
                return;
            }
        }
    }
    public Iterable<T> getAllEntities() {
        return java.util.Collections.unmodifiableCollection(byId.values());
    }
    public void add(T entity) {
        UUID key = entity.getUUID();
        if (byUuid.putIfAbsent(key, entity) == null) {
            byId.put(entity.getId(), entity);
        }
    }
    public void remove(T entity) {
        byUuid.remove(entity.getUUID());
        byId.remove(entity.getId());
    }
    public T getEntity(int id) {
        return byId.get(id);
    }
    public T getEntity(UUID id) {
        return byUuid.get(id);
    }
    public int count() {
        return byUuid.size();
    }
}
