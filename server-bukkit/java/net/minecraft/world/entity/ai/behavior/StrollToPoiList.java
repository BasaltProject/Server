// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.List;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.world.phys.Vec3;

public class StrollToPoiList {
    public static BehaviorControl<Villager> create(MemoryModuleType<List<GlobalPos>> strollToMemoryType, float speedModifier, int closeEnoughDist, int maxDistanceFromPoi,
            MemoryModuleType<GlobalPos> mustBeCloseToMemoryType) {
        long[] nextOkStartTime = {0L};
        return OneShots.<Villager>of(Set.of(MemoryModuleType.WALK_TARGET, strollToMemoryType, mustBeCloseToMemoryType), (level, body, timestamp) -> {
            var brain = body.getBrain();
            List<GlobalPos> strollTo = brain.getMemory(strollToMemoryType).orElse(null);
            GlobalPos stayCloseTo = brain.getMemory(mustBeCloseToMemoryType).orElse(null);
            if (strollTo == null || stayCloseTo == null || strollTo.isEmpty()) {
                return false;
            }
            GlobalPos targetPos = strollTo.get(level.getRandom().nextInt(strollTo.size()));
            if (targetPos == null || level.dimension() != targetPos.dimension() || !stayCloseTo.pos().closerToCenterThan(body.position(), maxDistanceFromPoi)) {
                return false;
            }
            if (timestamp > nextOkStartTime[0]) {
                brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(targetPos.pos(), speedModifier, closeEnoughDist));
                nextOkStartTime[0] = timestamp + 100L;
            }
            return true;
        });
    }
}
