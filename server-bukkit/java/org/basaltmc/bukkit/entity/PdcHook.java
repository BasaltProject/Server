package org.basaltmc.bukkit.entity;

import java.lang.reflect.Method;
import org.basaltmc.bukkit.Unsupported;
import org.bukkit.persistence.PersistentDataContainer;

/** Where an entity's persistent data container comes from, once the persistence layer is part of the build. */
final class PdcHook {
    private static final Method kEntity = find();

    private PdcHook() {}

    private static Method find() {
        try {
            return Class.forName("org.basaltmc.bukkit.persistence.PdcStores").getMethod("entity", String.class,
                int.class);
        } catch (ReflectiveOperationException absent) {
            return null;
        }
    }

    static PersistentDataContainer of(EntityCore core) {
        if (kEntity == null) {
            throw Unsupported.hit("Entity#getPersistentDataContainer");
        }
        try {
            return (PersistentDataContainer) kEntity.invoke(null, core.world.getName(), core.id);
        } catch (ReflectiveOperationException failure) {
            throw new IllegalStateException(failure);
        }
    }
}
