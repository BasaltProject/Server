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

public class SetWalkTargetFromBlockMemory {
    public static OneShot<Villager> create(MemoryModuleType<GlobalPos> memoryType, float speedModifier, int closeEnoughDist, int tooFarDistance, int tooLongUnreachableDuration) {
        return OneShots.<Villager>of(Set.of(MemoryModuleType.CANT_REACH_WALK_TARGET_SINCE, MemoryModuleType.WALK_TARGET, memoryType), (level, body, timestamp) -> {
            var brain = body.getBrain();
            GlobalPos targetPos = brain.getMemory(memoryType).orElse(null);
            if (targetPos == null || brain.hasMemoryValue(MemoryModuleType.WALK_TARGET)) {
                return false;
            }
            Optional<Long> cantReachSince = brain.getMemory(MemoryModuleType.CANT_REACH_WALK_TARGET_SINCE);
            if (targetPos.dimension() == level.dimension() && (cantReachSince.isEmpty() || level.getGameTime() - cantReachSince.get() <= tooLongUnreachableDuration)) {
                if (targetPos.pos().distManhattan(body.blockPosition()) > tooFarDistance) {
                    Vec3 towards = null;
                    int tries = 0;
                    while (towards == null || BlockPos.containing(towards).distManhattan(body.blockPosition()) > tooFarDistance) {
                        towards = net.minecraft.world.entity.ai.util.DefaultRandomPos.getPosTowards(body, 15, 7, Vec3.atBottomCenterOf(targetPos.pos()), (float) Math.PI / 2F);
                        if (++tries == 1000) {
                            body.releasePoi(memoryType);
                            brain.eraseMemory(memoryType);
                            brain.setMemory(MemoryModuleType.CANT_REACH_WALK_TARGET_SINCE, timestamp);
                            return true;
                        }
                    }
                    brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(towards, speedModifier, closeEnoughDist));
                } else if (targetPos.pos().distManhattan(body.blockPosition()) > closeEnoughDist) {
                    brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(targetPos.pos(), speedModifier, closeEnoughDist));
                }
            } else {
                body.releasePoi(memoryType);
                brain.eraseMemory(memoryType);
                brain.setMemory(MemoryModuleType.CANT_REACH_WALK_TARGET_SINCE, timestamp);
            }
            return true;
        });
    }
}
