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

public class GoToClosestVillage {
    public static BehaviorControl<Villager> create(float speedModifier, int closeEnoughDistance) {
        return OneShots.<Villager>of(Set.of(MemoryModuleType.WALK_TARGET), (level, body, timestamp) -> {
            if (body.getBrain().hasMemoryValue(MemoryModuleType.WALK_TARGET) || level.isVillage(body.blockPosition())) {
                return false;
            }
            var poiManager = level.getPoiManager();
            int sectionsToVillage = poiManager.sectionsToVillage(net.minecraft.core.SectionPos.of(body.blockPosition()));
            Vec3 targetPos = null;
            for (int j = 0; j < 5; ++j) {
                Vec3 landPos = net.minecraft.world.entity.ai.util.LandRandomPos.getPos(body, 15, 7, p -> -poiManager.sectionsToVillage(net.minecraft.core.SectionPos.of(p)));
                if (landPos != null) {
                    int landSections = poiManager.sectionsToVillage(net.minecraft.core.SectionPos.of(BlockPos.containing(landPos)));
                    if (landSections < sectionsToVillage) {
                        targetPos = landPos;
                        break;
                    }
                    if (landSections == sectionsToVillage) {
                        targetPos = landPos;
                    }
                }
            }
            if (targetPos != null) {
                body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(targetPos, speedModifier, closeEnoughDistance));
            }
            return true;
        });
    }
}
