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

public class YieldJobSite {
    public static BehaviorControl<Villager> create(float speedModifier) {
        return OneShots.<Villager>of(Set.of(MemoryModuleType.POTENTIAL_JOB_SITE, MemoryModuleType.JOB_SITE, MemoryModuleType.NEAREST_LIVING_ENTITIES,
            MemoryModuleType.WALK_TARGET, MemoryModuleType.LOOK_TARGET), (level, body, timestamp) -> {
            var brain = body.getBrain();
            GlobalPos potential = brain.getMemory(MemoryModuleType.POTENTIAL_JOB_SITE).orElse(null);
            List<LivingEntity> nearest = brain.getMemory(MemoryModuleType.NEAREST_LIVING_ENTITIES).orElse(null);
            if (potential == null || nearest == null || brain.hasMemoryValue(MemoryModuleType.JOB_SITE) || body.isBaby()
                || !body.getVillagerData().profession().is(net.minecraft.world.entity.npc.villager.VillagerProfession.NONE)) {
                return false;
            }
            BlockPos poiPos = potential.pos();
            var poiType = level.getPoiManager().getType(poiPos);
            if (poiType.isEmpty()) {
                return true;
            }
            nearest.stream().filter(v -> v instanceof Villager && v != body).map(v -> (Villager) v).filter(LivingEntity::isAlive)
                .filter(v -> nearbyWantsJobsite(poiType.get(), v, poiPos)).findFirst().ifPresent(nearby -> {
                    brain.eraseMemory(MemoryModuleType.WALK_TARGET);
                    brain.eraseMemory(MemoryModuleType.LOOK_TARGET);
                    brain.eraseMemory(MemoryModuleType.POTENTIAL_JOB_SITE);
                    if (nearby.getBrain().getMemory(MemoryModuleType.JOB_SITE).isEmpty()) {
                        BehaviorUtils.setWalkAndLookTargetMemories(nearby, poiPos, speedModifier, 1);
                        nearby.getBrain().setMemory(MemoryModuleType.POTENTIAL_JOB_SITE, GlobalPos.of(level.dimension(), poiPos));
                    }
                });
            return true;
        });
    }

    private static boolean nearbyWantsJobsite(net.minecraft.core.Holder<net.minecraft.world.entity.ai.village.poi.PoiType> type, Villager nearby, BlockPos poiPos) {
        if (nearby.getBrain().getMemory(MemoryModuleType.POTENTIAL_JOB_SITE).isPresent()) {
            return false;
        }
        Optional<GlobalPos> nearbyJobSite = nearby.getBrain().getMemory(MemoryModuleType.JOB_SITE);
        if (!nearby.getVillagerData().profession().value().heldJobSite().test(type)) {
            return false;
        }
        return nearbyJobSite.isEmpty() ? canReachPos(nearby, poiPos, type.value()) : nearbyJobSite.get().pos().equals(poiPos);
    }

    private static boolean canReachPos(PathfinderMob nearby, BlockPos poiPos, net.minecraft.world.entity.ai.village.poi.PoiType type) {
        var path = nearby.getNavigation().createPath(poiPos, type.validRange());
        return path != null && path.canReach();
    }
}
