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

public class PoiCompetitorScan {
    public static BehaviorControl<Villager> create() {
        return OneShots.<Villager>of(Set.of(MemoryModuleType.JOB_SITE, MemoryModuleType.NEAREST_LIVING_ENTITIES), (level, body, timestamp) -> {
            var brain = body.getBrain();
            GlobalPos pos = brain.getMemory(MemoryModuleType.JOB_SITE).orElse(null);
            List<LivingEntity> nearest = brain.getMemory(MemoryModuleType.NEAREST_LIVING_ENTITIES).orElse(null);
            if (pos == null || nearest == null) {
                return false;
            }
            level.getPoiManager().getType(pos.pos()).ifPresent(poiType -> nearest.stream().filter(v -> v instanceof Villager && v != body).map(v -> (Villager) v)
                .filter(LivingEntity::isAlive).filter(nearby -> competesForSameJobsite(pos, poiType, nearby)).reduce(body, PoiCompetitorScan::selectWinner));
            return true;
        });
    }

    private static Villager selectWinner(Villager first, Villager second) {
        Villager winner = first.getVillagerXp() > second.getVillagerXp() ? first : second;
        Villager loser = winner == first ? second : first;
        loser.getBrain().eraseMemory(MemoryModuleType.JOB_SITE);
        return winner;
    }

    private static boolean competesForSameJobsite(GlobalPos pos, net.minecraft.core.Holder<net.minecraft.world.entity.ai.village.poi.PoiType> poiType, Villager nearby) {
        Optional<GlobalPos> jobSite = nearby.getBrain().getMemory(MemoryModuleType.JOB_SITE);
        return jobSite.isPresent() && pos.equals(jobSite.get()) && nearby.getVillagerData().profession().value().heldJobSite().test(poiType);
    }
}
