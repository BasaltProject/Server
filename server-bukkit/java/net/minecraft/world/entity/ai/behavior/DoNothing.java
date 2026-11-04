// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class DoNothing implements BehaviorControl<LivingEntity> {
    private final int minDuration;
    private final int maxDuration;
    private Behavior.Status status = Behavior.Status.STOPPED;
    private long endTimestamp;

    public DoNothing(int minDuration, int maxDuration) {
        this.minDuration = minDuration;
        this.maxDuration = maxDuration;
    }

    public Behavior.Status getStatus() {
        return status;
    }

    public Set<MemoryModuleType<?>> getRequiredMemories() {
        return Set.of();
    }

    public final boolean tryStart(ServerLevel level, LivingEntity body, long timestamp) {
        status = Behavior.Status.RUNNING;
        endTimestamp = timestamp + minDuration + level.getRandom().nextInt(maxDuration + 1 - minDuration);
        return true;
    }

    public final void tickOrStop(ServerLevel level, LivingEntity body, long timestamp) {
        if (timestamp > endTimestamp) {
            doStop(level, body, timestamp);
        }
    }

    public final void doStop(ServerLevel level, LivingEntity body, long timestamp) {
        status = Behavior.Status.STOPPED;
    }

    public String debugString() {
        return getClass().getSimpleName();
    }
}
