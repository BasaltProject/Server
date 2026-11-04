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

public class AssignProfessionFromJobSite {
    public static BehaviorControl<Villager> create() {
        return OneShots.<Villager>of(Set.of(MemoryModuleType.POTENTIAL_JOB_SITE, MemoryModuleType.JOB_SITE), (level, body, timestamp) -> {
            var brain = body.getBrain();
            GlobalPos pos = brain.getMemory(MemoryModuleType.POTENTIAL_JOB_SITE).orElse(null);
            if (pos == null || !pos.pos().closerToCenterThan(body.position(), 2.0)) {
                return false;
            }
            brain.eraseMemory(MemoryModuleType.POTENTIAL_JOB_SITE);
            brain.setMemory(MemoryModuleType.JOB_SITE, pos);
            level.broadcastEntityEvent(body, (byte) 14);
            if (!body.getVillagerData().profession().is(net.minecraft.world.entity.npc.villager.VillagerProfession.NONE)) {
                return true;
            }
            ServerLevel poiLevel = level.getServer().getLevel(pos.dimension());
            Optional.ofNullable(poiLevel).flatMap(l -> l.getPoiManager().getType(pos.pos()))
                .flatMap(poiType -> net.minecraft.core.registries.BuiltInRegistries.VILLAGER_PROFESSION.listElements()
                    .filter(profession -> profession.value().heldJobSite().test(poiType)).findFirst())
                .ifPresent(profession -> {
                    body.setVillagerData(body.getVillagerData().withProfession(profession));
                    body.refreshBrain(level);
                });
            return true;
        });
    }
}
