// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.phys.Vec3;

public class SetClosestHomeAsWalkTarget {
    private static final int CACHE_TIMEOUT = 40;
    private static final int BATCH_SIZE = 5;
    private static final int RATE = 20;
    private static final int OK_DISTANCE_SQR = 4;

    public static BehaviorControl<PathfinderMob> create(float speedModifier) {
        it.unimi.dsi.fastutil.longs.Long2LongMap batchCache = new it.unimi.dsi.fastutil.longs.Long2LongOpenHashMap();
        long[] lastUpdate = {0L};
        return OneShots.<PathfinderMob>of(Set.of(MemoryModuleType.WALK_TARGET, MemoryModuleType.HOME), (level, body, timestamp) -> {
            Brain<?> brain = body.getBrain();
            if (brain.hasMemoryValue(MemoryModuleType.WALK_TARGET) || brain.hasMemoryValue(MemoryModuleType.HOME) || level.getGameTime() - lastUpdate[0] < RATE) {
                return false;
            }
            var poiManager = level.getPoiManager();
            Optional<BlockPos> closest = poiManager.findClosest(p -> p.is(net.minecraft.world.entity.ai.village.poi.PoiTypes.HOME), body.blockPosition(), 48,
                net.minecraft.world.entity.ai.village.poi.PoiManager.Occupancy.ANY);
            if (closest.isEmpty() || closest.get().distSqr(body.blockPosition()) <= OK_DISTANCE_SQR) {
                return false;
            }
            int[] tried = {0};
            lastUpdate[0] = level.getGameTime() + level.getRandom().nextInt(RATE);
            java.util.function.Predicate<BlockPos> cacheTest = pos -> {
                long key = pos.asLong();
                if (batchCache.containsKey(key) || ++tried[0] >= BATCH_SIZE) {
                    return false;
                }
                batchCache.put(key, lastUpdate[0] + CACHE_TIMEOUT);
                return true;
            };
            var pois = poiManager.findAllWithType(p -> p.is(net.minecraft.world.entity.ai.village.poi.PoiTypes.HOME), cacheTest, body.blockPosition(), 48,
                net.minecraft.world.entity.ai.village.poi.PoiManager.Occupancy.ANY).collect(java.util.stream.Collectors.toSet());
            var path = AcquirePoi.findPathToPois(body, pois);
            if (path != null && path.canReach()) {
                BlockPos target = path.getTarget();
                if (poiManager.getType(target).isPresent()) {
                    brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(target, speedModifier, 1));
                }
            } else if (tried[0] < BATCH_SIZE) {
                batchCache.long2LongEntrySet().removeIf(entry -> entry.getLongValue() < lastUpdate[0]);
            }
            return true;
        });
    }
}
