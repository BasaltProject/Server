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

public class LocateHidingPlace {
    public static OneShot<LivingEntity> create(int radius, float speedModifier, int closeEnoughDist) {
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.WALK_TARGET, MemoryModuleType.HOME, MemoryModuleType.HIDING_PLACE, MemoryModuleType.PATH,
            MemoryModuleType.LOOK_TARGET, MemoryModuleType.BREED_TARGET, MemoryModuleType.INTERACTION_TARGET), (level, body, timestamp) -> {
            var brain = body.getBrain();
            if (brain.hasMemoryValue(MemoryModuleType.WALK_TARGET)) {
                return false;
            }
            var poi = level.getPoiManager();
            poi.find(p -> p.is(net.minecraft.world.entity.ai.village.poi.PoiTypes.HOME), blockPos -> true, body.blockPosition(), closeEnoughDist + 1,
                    net.minecraft.world.entity.ai.village.poi.PoiManager.Occupancy.ANY).filter(p -> p.closerToCenterThan(body.position(), closeEnoughDist))
                .or(() -> poi.getRandom(p -> p.is(net.minecraft.world.entity.ai.village.poi.PoiTypes.HOME), blockPos -> true,
                    net.minecraft.world.entity.ai.village.poi.PoiManager.Occupancy.ANY, body.blockPosition(), radius, body.getRandom()))
                .or(() -> brain.getMemory(MemoryModuleType.HOME).map(GlobalPos::pos)).ifPresent(pos -> {
                    brain.eraseMemory(MemoryModuleType.PATH);
                    brain.eraseMemory(MemoryModuleType.LOOK_TARGET);
                    brain.eraseMemory(MemoryModuleType.BREED_TARGET);
                    brain.eraseMemory(MemoryModuleType.INTERACTION_TARGET);
                    brain.setMemory(MemoryModuleType.HIDING_PLACE, GlobalPos.of(level.dimension(), pos));
                    if (!pos.closerToCenterThan(body.position(), closeEnoughDist)) {
                        brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(pos, speedModifier, closeEnoughDist));
                    }
                });
            return true;
        });
    }
}
