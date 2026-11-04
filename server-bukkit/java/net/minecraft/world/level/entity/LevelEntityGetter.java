// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.entity;

import java.util.UUID;
import org.basaltmc.bukkit.Unsupported;
import java.util.function.Consumer;
import net.minecraft.util.AbortableIterationConsumer;
import net.minecraft.world.phys.AABB;
public interface LevelEntityGetter<T extends EntityAccess> {
    public default EntityAccess get(UUID id) {
        throw Unsupported.hit("net.minecraft.world.level.entity.LevelEntityGetter.get");
    }
    default T get(int id) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.LevelEntityGetter#get");
    }
    default Iterable<T> getAll() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.LevelEntityGetter#getAll");
    }
    default <U extends T> void get(EntityTypeTest<T, U> type, AbortableIterationConsumer<U> consumer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.LevelEntityGetter#get");
    }
    default void get(AABB bb, Consumer<T> output) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.LevelEntityGetter#get");
    }
    default <U extends T> void get(EntityTypeTest<T, U> type, AABB bb, AbortableIterationConsumer<U> consumer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.entity.LevelEntityGetter#get");
    }
}
