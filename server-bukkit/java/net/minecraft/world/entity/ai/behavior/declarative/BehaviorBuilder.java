// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior.declarative;

import java.util.Set;
import java.util.function.Predicate;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.behavior.BehaviorControl;
import net.minecraft.world.entity.ai.behavior.OneShot;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public final class BehaviorBuilder {
    private BehaviorBuilder() {
    }

    public static <E extends LivingEntity> OneShot<E> triggerIf(Predicate<E> predicate) {
        return new OneShot<E>() {
            public Set<MemoryModuleType<?>> getRequiredMemories() {
                return Set.of();
            }

            public boolean trigger(ServerLevel level, E entity, long timestamp) {
                return predicate.test(entity);
            }
        };
    }

    public static <E extends LivingEntity> OneShot<E> triggerIf(Predicate<E> predicate, BehaviorControl<? super E> then) {
        return new OneShot<E>() {
            public Set<MemoryModuleType<?>> getRequiredMemories() {
                return then.getRequiredMemories();
            }

            public boolean trigger(ServerLevel level, E entity, long timestamp) {
                return predicate.test(entity) && then.tryStart(level, entity, timestamp);
            }
        };
    }
}
