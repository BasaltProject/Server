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

public class SetRaidStatus {
    public static BehaviorControl<LivingEntity> create() {
        return OneShots.<LivingEntity>of(Set.of(), (level, body, timestamp) -> {
            if (level.getRandom().nextInt(20) != 0) {
                return false;
            }
            Brain<?> brain = body.getBrain();
            var raid = level.getRaidAt(body.blockPosition());
            if (raid != null) {
                if (raid.hasFirstWaveSpawned() && !raid.isBetweenWaves()) {
                    brain.setDefaultActivity(Activity.RAID);
                    brain.setActiveActivityIfPossible(Activity.RAID);
                } else {
                    brain.setDefaultActivity(Activity.PRE_RAID);
                    brain.setActiveActivityIfPossible(Activity.PRE_RAID);
                }
            }
            return true;
        });
    }
}
