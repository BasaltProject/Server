// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import com.mojang.datafixers.util.Pair;
import java.util.List;
import java.util.Map;
import java.util.Set;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;

public class RunOne<E extends LivingEntity> extends GateBehavior<E> {
    public RunOne(List<Pair<? extends BehaviorControl<? super E>, Integer>> weightedBehaviors) {
        this(Map.of(), weightedBehaviors);
    }

    public RunOne(Map<MemoryModuleType<?>, MemoryStatus> entryCondition, List<Pair<? extends BehaviorControl<? super E>, Integer>> weightedBehaviors) {
        super(entryCondition, Set.of(), OrderPolicy.SHUFFLED, RunningPolicy.RUN_ONE, weightedBehaviors);
    }
}
