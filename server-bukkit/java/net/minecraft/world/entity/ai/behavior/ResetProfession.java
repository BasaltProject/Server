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

public class ResetProfession {
    public static BehaviorControl<Villager> create() {
        return OneShots.<Villager>of(Set.of(MemoryModuleType.JOB_SITE), (level, body, timestamp) -> {
            if (body.getBrain().hasMemoryValue(MemoryModuleType.JOB_SITE)) {
                return false;
            }
            var data = body.getVillagerData();
            boolean canBeFired = !data.profession().is(net.minecraft.world.entity.npc.villager.VillagerProfession.NONE)
                && !data.profession().is(net.minecraft.world.entity.npc.villager.VillagerProfession.NITWIT);
            if (canBeFired && body.getVillagerXp() == 0 && data.level() <= 1) {
                body.setVillagerData(body.getVillagerData().withProfession(level.registryAccess(), net.minecraft.world.entity.npc.villager.VillagerProfession.NONE));
                body.refreshBrain(level);
                return true;
            }
            return false;
        });
    }
}
