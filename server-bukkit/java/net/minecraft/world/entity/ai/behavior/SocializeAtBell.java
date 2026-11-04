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

public class SocializeAtBell {
    private static final float SPEED_MODIFIER = 0.3F;

    public static OneShot<LivingEntity> create() {
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.WALK_TARGET, MemoryModuleType.LOOK_TARGET, MemoryModuleType.MEETING_POINT,
            MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES, MemoryModuleType.INTERACTION_TARGET), (level, body, timestamp) -> {
            var brain = body.getBrain();
            GlobalPos memory = brain.getMemory(MemoryModuleType.MEETING_POINT).orElse(null);
            var visible = brain.getMemory(MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES).orElse(null);
            if (memory == null || visible == null || brain.hasMemoryValue(MemoryModuleType.INTERACTION_TARGET)) {
                return false;
            }
            if (level.getRandom().nextInt(100) == 0 && level.dimension() == memory.dimension() && memory.pos().closerToCenterThan(body.position(), 4.0)
                && visible.contains(mob -> mob.is(net.minecraft.world.entity.EntityTypes.VILLAGER))) {
                visible.findClosest(mob -> mob.is(net.minecraft.world.entity.EntityTypes.VILLAGER) && mob.distanceToSqr(body) <= 32.0).ifPresent(mob -> {
                    brain.setMemory(MemoryModuleType.INTERACTION_TARGET, mob);
                    brain.setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(mob, true));
                    brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(new EntityTracker(mob, false), SPEED_MODIFIER, 1));
                });
                return true;
            }
            return false;
        });
    }
}
