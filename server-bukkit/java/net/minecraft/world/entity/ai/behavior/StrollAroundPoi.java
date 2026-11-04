// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import net.minecraft.core.GlobalPos;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.ai.util.LandRandomPos;
import net.minecraft.world.phys.Vec3;

public class StrollAroundPoi {
    private static final int MIN_TIME_BETWEEN_STROLLS = 180;
    private static final int STROLL_MAX_XZ_DIST = 8;
    private static final int STROLL_MAX_Y_DIST = 6;

    public static OneShot<PathfinderMob> create(MemoryModuleType<GlobalPos> memoryType, float speedModifier, int maxDistanceFromPoi) {
        long[] nextOkStartTime = {0L};
        return OneShots.<PathfinderMob>of(Set.of(MemoryModuleType.WALK_TARGET, memoryType), (level, body, timestamp) -> {
            GlobalPos pos = body.getBrain().getMemory(memoryType).orElse(null);
            if (pos == null || level.dimension() != pos.dimension() || !pos.pos().closerToCenterThan(body.position(), maxDistanceFromPoi)) {
                return false;
            }
            if (timestamp > nextOkStartTime[0]) {
                Vec3 landPos = LandRandomPos.getPos(body, STROLL_MAX_XZ_DIST, STROLL_MAX_Y_DIST);
                if (landPos != null) {
                    body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(landPos, speedModifier, 1));
                } else {
                    body.getBrain().eraseMemory(MemoryModuleType.WALK_TARGET);
                }
                nextOkStartTime[0] = timestamp + MIN_TIME_BETWEEN_STROLLS;
            }
            return true;
        });
    }
}
