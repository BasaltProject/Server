// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import com.mojang.datafixers.util.Pair;
import java.util.List;
import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.behavior.declarative.Trigger;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class TriggerGate {
    public static <E extends LivingEntity> OneShot<E> triggerOneShuffled(List<Pair<? extends Trigger<? super E>, Integer>> weightedTriggers) {
        return triggerGate(weightedTriggers, GateBehavior.OrderPolicy.SHUFFLED, GateBehavior.RunningPolicy.RUN_ONE);
    }

    public static <E extends LivingEntity> OneShot<E> triggerGate(List<Pair<? extends Trigger<? super E>, Integer>> weightedBehaviors,
            GateBehavior.OrderPolicy orderPolicy, GateBehavior.RunningPolicy runningPolicy) {
        ShufflingList<Trigger<? super E>> triggers = new ShufflingList<>();
        weightedBehaviors.forEach(entry -> triggers.add(entry.getFirst(), entry.getSecond()));
        return new OneShot<E>() {
            public Set<MemoryModuleType<?>> getRequiredMemories() {
                return Set.of();
            }

            public boolean trigger(ServerLevel level, E body, long timestamp) {
                orderPolicy.apply(triggers);
                boolean fired = false;
                for (Trigger<? super E> trigger : triggers) {
                    if (trigger.trigger(level, body, timestamp)) {
                        fired = true;
                        if (runningPolicy == GateBehavior.RunningPolicy.RUN_ONE) {
                            break;
                        }
                    }
                }
                return fired;
            }
        };
    }
}
