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

public class VillageBoundRandomStroll {
    private static final int MAX_XZ_DIST = 10;
    private static final int MAX_Y_DIST = 7;

    public static OneShot<PathfinderMob> create(float speedModifier) {
        return create(speedModifier, MAX_XZ_DIST, MAX_Y_DIST);
    }

    public static OneShot<PathfinderMob> create(float speedModifier, int maxXyDist, int maxYDist) {
        return OneShots.<PathfinderMob>of(Set.of(MemoryModuleType.WALK_TARGET), (level, body, timestamp) -> {
            if (body.getBrain().hasMemoryValue(MemoryModuleType.WALK_TARGET)) {
                return false;
            }
            BlockPos bodyPos = body.blockPosition();
            Vec3 landPos;
            if (level.isVillage(bodyPos)) {
                landPos = net.minecraft.world.entity.ai.util.LandRandomPos.getPos(body, maxXyDist, maxYDist);
            } else {
                var sectionPos = net.minecraft.core.SectionPos.of(bodyPos);
                var optimal = BehaviorUtils.findSectionClosestToVillage(level, sectionPos, 2);
                landPos = optimal != sectionPos
                    ? net.minecraft.world.entity.ai.util.DefaultRandomPos.getPosTowards(body, maxXyDist, maxYDist, Vec3.atBottomCenterOf(optimal.center()), (float) Math.PI / 2F)
                    : net.minecraft.world.entity.ai.util.LandRandomPos.getPos(body, maxXyDist, maxYDist);
            }
            if (landPos != null) {
                body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(landPos, speedModifier, 0));
            } else {
                body.getBrain().eraseMemory(MemoryModuleType.WALK_TARGET);
            }
            return true;
        });
    }
}
