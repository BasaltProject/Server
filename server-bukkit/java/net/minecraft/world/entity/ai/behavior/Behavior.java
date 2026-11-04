// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Map;
import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;

public abstract class Behavior<E extends LivingEntity> implements BehaviorControl<E> {
    public static final int DEFAULT_DURATION = 60;
    protected final Map<MemoryModuleType<?>, MemoryStatus> entryCondition;
    private Status status = Status.STOPPED;
    private long endTimestamp;
    private final int minDuration;
    private final int maxDuration;

    public Behavior(Map<MemoryModuleType<?>, MemoryStatus> entryCondition) {
        this(entryCondition, DEFAULT_DURATION);
    }

    public Behavior(Map<MemoryModuleType<?>, MemoryStatus> entryCondition, int timeOutDuration) {
        this(entryCondition, timeOutDuration, timeOutDuration);
    }

    public Behavior(Map<MemoryModuleType<?>, MemoryStatus> entryCondition, int minDuration, int maxDuration) {
        this.minDuration = minDuration;
        this.maxDuration = maxDuration;
        this.entryCondition = entryCondition;
    }

    @Override
    public Status getStatus() {
        return status;
    }

    @Override
    public Set<MemoryModuleType<?>> getRequiredMemories() {
        return entryCondition.keySet();
    }

    @Override
    public final boolean tryStart(ServerLevel level, E body, long timestamp) {
        if (hasRequiredMemories(body) && checkExtraStartConditions(level, body)) {
            status = Status.RUNNING;
            int duration = minDuration + level.getRandom().nextInt(maxDuration + 1 - minDuration);
            endTimestamp = timestamp + duration;
            start(level, body, timestamp);
            return true;
        }
        return false;
    }

    protected void start(ServerLevel level, E body, long timestamp) {
    }

    @Override
    public final void tickOrStop(ServerLevel level, E body, long timestamp) {
        if (!timedOut(timestamp) && canStillUse(level, body, timestamp)) {
            tick(level, body, timestamp);
        } else {
            doStop(level, body, timestamp);
        }
    }

    protected void tick(ServerLevel level, E body, long timestamp) {
    }

    @Override
    public final void doStop(ServerLevel level, E body, long timestamp) {
        status = Status.STOPPED;
        stop(level, body, timestamp);
    }

    protected void stop(ServerLevel level, E body, long timestamp) {
    }

    protected boolean canStillUse(ServerLevel level, E body, long timestamp) {
        return false;
    }

    protected boolean timedOut(long timestamp) {
        return timestamp > endTimestamp;
    }

    protected boolean checkExtraStartConditions(ServerLevel level, E body) {
        return true;
    }

    @Override
    public String debugString() {
        return getClass().getSimpleName();
    }

    protected boolean hasRequiredMemories(E body) {
        for (Map.Entry<MemoryModuleType<?>, MemoryStatus> entry : entryCondition.entrySet()) {
            if (!body.getBrain().checkMemory(entry.getKey(), entry.getValue())) {
                return false;
            }
        }
        return true;
    }

    public static enum Status {
        STOPPED, RUNNING;
    }
}
