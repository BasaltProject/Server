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

public class ValidateNearbyPoi {
    private static final int MAX_DISTANCE = 16;

    public static BehaviorControl<LivingEntity> create(java.util.function.Predicate<net.minecraft.core.Holder<net.minecraft.world.entity.ai.village.poi.PoiType>> poiType,
            MemoryModuleType<GlobalPos> memoryType) {
        return OneShots.<LivingEntity>of(Set.of(memoryType), (level, body, timestamp) -> {
            GlobalPos globalPos = body.getBrain().getMemory(memoryType).orElse(null);
            if (globalPos == null) {
                return false;
            }
            BlockPos pos = globalPos.pos();
            if (level.dimension() != globalPos.dimension() || !pos.closerToCenterThan(body.position(), MAX_DISTANCE)) {
                return false;
            }
            ServerLevel poiLevel = level.getServer().getLevel(globalPos.dimension());
            if (poiLevel != null && poiLevel.getPoiManager().exists(pos, poiType)) {
                if (bedIsOccupied(poiLevel, pos, body)) {
                    body.getBrain().eraseMemory(memoryType);
                    if (!bedIsOccupiedByVillager(poiLevel, pos)) {
                        level.getPoiManager().release(pos);
                    }
                }
            } else {
                body.getBrain().eraseMemory(memoryType);
            }
            return true;
        });
    }

    private static boolean bedIsOccupied(ServerLevel poiLevel, BlockPos poiPos, LivingEntity body) {
        var state = poiLevel.getBlockState(poiPos);
        return state.is(net.minecraft.tags.BlockTags.VILLAGERS_CAN_SLEEP_ON_BED) && state.getValue(net.minecraft.world.level.block.BedBlock.OCCUPIED) && !body.isSleeping();
    }

    private static boolean bedIsOccupiedByVillager(ServerLevel poiLevel, BlockPos poiPos) {
        return !poiLevel.getEntitiesOfClass(Villager.class, new net.minecraft.world.phys.AABB(poiPos), LivingEntity::isSleeping).isEmpty();
    }
}
