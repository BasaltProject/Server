// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.entity;

import java.util.UUID;
import java.util.function.Consumer;
import net.minecraft.util.AbortableIterationConsumer;
import net.minecraft.world.phys.AABB;
public interface LevelEntityGetter<T extends EntityAccess> {
    T get(UUID id);
    T get(int id);
    Iterable<T> getAll();
    <U extends T> void get(EntityTypeTest<T, U> type, AbortableIterationConsumer<U> consumer);
    void get(AABB bb, Consumer<T> output);
    <U extends T> void get(EntityTypeTest<T, U> type, AABB bb, AbortableIterationConsumer<U> consumer);
}
