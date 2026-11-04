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

public class SetHiddenState {
    private static final int HIDE_TIMEOUT = 300;

    public static BehaviorControl<LivingEntity> create(int seconds, int closeEnoughDist) {
        int stayHiddenTicks = seconds * 20;
        int[] ticksHidden = {0};
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.HIDING_PLACE, MemoryModuleType.HEARD_BELL_TIME), (level, body, timestamp) -> {
            var brain = body.getBrain();
            GlobalPos hiding = brain.getMemory(MemoryModuleType.HIDING_PLACE).orElse(null);
            Long heard = brain.getMemory(MemoryModuleType.HEARD_BELL_TIME).orElse(null);
            if (hiding == null || heard == null) {
                return false;
            }
            boolean timedOut = heard + HIDE_TIMEOUT <= timestamp;
            if (ticksHidden[0] <= stayHiddenTicks && !timedOut) {
                if (hiding.pos().closerThan(body.blockPosition(), closeEnoughDist)) {
                    ticksHidden[0]++;
                }
            } else {
                brain.eraseMemory(MemoryModuleType.HEARD_BELL_TIME);
                brain.eraseMemory(MemoryModuleType.HIDING_PLACE);
                brain.updateActivityFromSchedule(level.environmentAttributes(), level.getGameTime(), body.position());
                ticksHidden[0] = 0;
            }
            return true;
        });
    }
}
