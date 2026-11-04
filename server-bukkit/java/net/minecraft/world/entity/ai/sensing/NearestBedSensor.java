// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;

import com.mojang.datafixers.util.Pair;
import it.unimi.dsi.fastutil.longs.Long2LongMap;
import it.unimi.dsi.fastutil.longs.Long2LongOpenHashMap;
import java.util.Set;
import java.util.function.Predicate;
import java.util.stream.Collectors;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.behavior.AcquirePoi;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.village.poi.PoiManager;
import net.minecraft.world.entity.ai.village.poi.PoiType;
import net.minecraft.world.entity.ai.village.poi.PoiTypes;
import net.minecraft.world.level.pathfinder.Path;

public class NearestBedSensor extends Sensor<Mob> {
    private static final int CACHE_TIMEOUT = 40;
    private static final int BATCH_SIZE = 5;
    private static final int RATE = 20;
    private final Long2LongMap batchCache = new Long2LongOpenHashMap();
    private int triedCount;
    private long lastUpdate;

    public NearestBedSensor() {
        super(RATE);
    }

    public Set<MemoryModuleType<?>> requires() {
        return Set.of(MemoryModuleType.NEAREST_BED);
    }

    protected void doTick(ServerLevel level, Mob body) {
        if (!body.isBaby()) {
            return;
        }
        triedCount = 0;
        lastUpdate = level.getGameTime() + level.getRandom().nextInt(RATE);
        PoiManager poiManager = level.getPoiManager();
        Predicate<BlockPos> notTriedYet = pos -> {
            long key = pos.asLong();
            if (batchCache.containsKey(key) || ++triedCount >= BATCH_SIZE) {
                return false;
            }
            batchCache.put(key, lastUpdate + CACHE_TIMEOUT);
            return true;
        };
        Set<Pair<Holder<PoiType>, BlockPos>> pois = poiManager
            .findAllWithType(type -> type.is(PoiTypes.HOME), notTriedYet, body.blockPosition(), AcquirePoi.SCAN_RANGE, PoiManager.Occupancy.ANY)
            .collect(Collectors.toSet());
        Path path = AcquirePoi.findPathToPois(body, pois);
        if (path != null && path.canReach()) {
            BlockPos target = path.getTarget();
            if (poiManager.getType(target).isPresent()) {
                body.getBrain().setMemory(MemoryModuleType.NEAREST_BED, target);
            }
        } else if (triedCount < BATCH_SIZE) {
            batchCache.long2LongEntrySet().removeIf(entry -> entry.getLongValue() < lastUpdate);
        }
    }
}
