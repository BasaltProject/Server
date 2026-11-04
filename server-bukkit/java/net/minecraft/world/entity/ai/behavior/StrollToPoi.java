// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import net.minecraft.core.GlobalPos;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;

public class StrollToPoi {
    public static BehaviorControl<PathfinderMob> create(MemoryModuleType<GlobalPos> memoryType, float speedModifier, int closeEnoughDist, int maxDistanceFromPoi) {
        long[] nextOkStartTime = {0L};
        return OneShots.<PathfinderMob>of(Set.of(MemoryModuleType.WALK_TARGET, memoryType), (level, body, timestamp) -> {
            GlobalPos pos = body.getBrain().getMemory(memoryType).orElse(null);
            if (pos == null || level.dimension() != pos.dimension() || !pos.pos().closerToCenterThan(body.position(), maxDistanceFromPoi)) {
                return false;
            }
            if (timestamp > nextOkStartTime[0]) {
                body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(pos.pos(), speedModifier, closeEnoughDist));
                nextOkStartTime[0] = timestamp + 80L;
            }
            return true;
        });
    }
}
