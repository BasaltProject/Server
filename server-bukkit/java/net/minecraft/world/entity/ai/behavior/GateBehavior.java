// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import com.mojang.datafixers.util.Pair;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Stream;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;

public class GateBehavior<E extends LivingEntity> implements BehaviorControl<E> {
    private final Map<MemoryModuleType<?>, MemoryStatus> entryCondition;
    private final Set<MemoryModuleType<?>> exitErasedMemories;
    private final OrderPolicy orderPolicy;
    private final RunningPolicy runningPolicy;
    private final ShufflingList<BehaviorControl<? super E>> behaviors = new ShufflingList<>();
    private Behavior.Status status = Behavior.Status.STOPPED;

    public GateBehavior(Map<MemoryModuleType<?>, MemoryStatus> entryCondition, Set<MemoryModuleType<?>> exitErasedMemories, OrderPolicy orderPolicy,
            RunningPolicy runningPolicy, List<Pair<? extends BehaviorControl<? super E>, Integer>> behaviors) {
        this.entryCondition = entryCondition;
        this.exitErasedMemories = exitErasedMemories;
        this.orderPolicy = orderPolicy;
        this.runningPolicy = runningPolicy;
        for (Pair<? extends BehaviorControl<? super E>, Integer> entry : behaviors) {
            this.behaviors.add(entry.getFirst(), entry.getSecond());
        }
    }

    public Behavior.Status getStatus() {
        return status;
    }

    public Set<MemoryModuleType<?>> getRequiredMemories() {
        return entryCondition.keySet();
    }

    private boolean hasRequiredMemories(E body) {
        for (Map.Entry<MemoryModuleType<?>, MemoryStatus> entry : entryCondition.entrySet()) {
            if (!body.getBrain().checkMemory(entry.getKey(), entry.getValue())) {
                return false;
            }
        }
        return true;
    }

    public final boolean tryStart(ServerLevel level, E body, long timestamp) {
        if (!hasRequiredMemories(body)) {
            return false;
        }
        status = Behavior.Status.RUNNING;
        orderPolicy.apply(behaviors);
        runningPolicy.apply(behaviors.stream(), level, body, timestamp);
        return true;
    }

    public final void tickOrStop(ServerLevel level, E body, long timestamp) {
        behaviors.stream().filter(behavior -> behavior.getStatus() == Behavior.Status.RUNNING).forEach(behavior -> behavior.tickOrStop(level, body, timestamp));
        if (behaviors.stream().noneMatch(behavior -> behavior.getStatus() == Behavior.Status.RUNNING)) {
            doStop(level, body, timestamp);
        }
    }

    public final void doStop(ServerLevel level, E body, long timestamp) {
        status = Behavior.Status.STOPPED;
        behaviors.stream().filter(behavior -> behavior.getStatus() == Behavior.Status.RUNNING).forEach(behavior -> behavior.doStop(level, body, timestamp));
        exitErasedMemories.forEach(body.getBrain()::eraseMemory);
    }

    public String debugString() {
        return getClass().getSimpleName();
    }

    public static enum OrderPolicy {
        ORDERED, SHUFFLED;

        public void apply(ShufflingList<?> list) {
            if (this == SHUFFLED) {
                list.shuffle();
            }
        }
    }

    public static enum RunningPolicy {
        RUN_ONE, TRY_ALL;

        public <E extends LivingEntity> void apply(Stream<BehaviorControl<? super E>> behaviors, ServerLevel level, E body, long timestamp) {
            Stream<BehaviorControl<? super E>> stopped = behaviors.filter(behavior -> behavior.getStatus() == Behavior.Status.STOPPED);
            if (this == RUN_ONE) {
                stopped.filter(behavior -> behavior.tryStart(level, body, timestamp)).findFirst();
            } else {
                stopped.forEach(behavior -> behavior.tryStart(level, body, timestamp));
            }
        }
    }
}
