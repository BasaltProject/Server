// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
public final class InertBehavior<E extends LivingEntity> extends OneShot<E> {
    public Set<MemoryModuleType<?>> getRequiredMemories() {
        return Set.of();
    }
    public boolean trigger(ServerLevel level, E body, long timestamp) {
        return false;
    }
}
