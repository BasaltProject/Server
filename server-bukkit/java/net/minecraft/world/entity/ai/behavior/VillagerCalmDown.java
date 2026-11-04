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

public class VillagerCalmDown {
    private static final int SAFE_DISTANCE_FROM_DANGER = 36;

    public static BehaviorControl<LivingEntity> create() {
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.HURT_BY, MemoryModuleType.HURT_BY_ENTITY, MemoryModuleType.NEAREST_HOSTILE), (level, body, timestamp) -> {
            var brain = body.getBrain();
            boolean feelScared = brain.hasMemoryValue(MemoryModuleType.HURT_BY) || brain.hasMemoryValue(MemoryModuleType.NEAREST_HOSTILE)
                || brain.getMemory(MemoryModuleType.HURT_BY_ENTITY).filter(entity -> entity.distanceToSqr(body) <= SAFE_DISTANCE_FROM_DANGER).isPresent();
            if (!feelScared) {
                brain.eraseMemory(MemoryModuleType.HURT_BY);
                brain.eraseMemory(MemoryModuleType.HURT_BY_ENTITY);
                brain.updateActivityFromSchedule(level.environmentAttributes(), level.getGameTime(), body.position());
            }
            return true;
        });
    }
}
