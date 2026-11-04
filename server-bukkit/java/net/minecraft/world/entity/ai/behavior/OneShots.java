// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public final class OneShots {
    public interface Body<E extends LivingEntity> {
        boolean run(ServerLevel level, E body, long timestamp);
    }

    private OneShots() {
    }

    public static <E extends LivingEntity> OneShot<E> of(Set<MemoryModuleType<?>> required, Body<E> body) {
        return new OneShot<E>() {
            public Set<MemoryModuleType<?>> getRequiredMemories() {
                return required;
            }

            public boolean trigger(ServerLevel level, E entity, long timestamp) {
                return body.run(level, entity, timestamp);
            }
        };
    }
}
