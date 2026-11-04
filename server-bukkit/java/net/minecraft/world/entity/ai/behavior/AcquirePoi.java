// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import com.mojang.datafixers.util.Pair;
import it.unimi.dsi.fastutil.longs.Long2ObjectMap;
import it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap;
import java.util.HashSet;
import java.util.Optional;
import java.util.Set;
import java.util.function.BiPredicate;
import java.util.function.Predicate;
import java.util.stream.Collectors;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.village.poi.PoiManager;
import net.minecraft.world.entity.ai.village.poi.PoiType;
import net.minecraft.world.level.pathfinder.Path;

public class AcquirePoi {
    public static final int SCAN_RANGE = 48;
    private static final int BATCH_SIZE = 5;
    private static final int RATE = 20;

    public static BehaviorControl<PathfinderMob> create(Predicate<Holder<PoiType>> poiType, MemoryModuleType<GlobalPos> memoryToAcquire,
            boolean onlyIfAdult, Optional<Byte> onPoiAcquisitionEvent, BiPredicate<ServerLevel, BlockPos> validPoi) {
        return create(poiType, memoryToAcquire, memoryToAcquire, onlyIfAdult, onPoiAcquisitionEvent, validPoi);
    }

    public static BehaviorControl<PathfinderMob> create(Predicate<Holder<PoiType>> poiType, MemoryModuleType<GlobalPos> memoryToAcquire,
            boolean onlyIfAdult, Optional<Byte> onPoiAcquisitionEvent) {
        return create(poiType, memoryToAcquire, memoryToAcquire, onlyIfAdult, onPoiAcquisitionEvent, (level, pos) -> true);
    }

    public static BehaviorControl<PathfinderMob> create(Predicate<Holder<PoiType>> poiType, MemoryModuleType<GlobalPos> memoryToValidate,
            MemoryModuleType<GlobalPos> memoryToAcquire, boolean onlyIfAdult, Optional<Byte> onPoiAcquisitionEvent, BiPredicate<ServerLevel, BlockPos> validPoi) {
        long[] nextScheduledStart = {0L};
        Long2ObjectMap<JitteredLinearRetry> batchCache = new Long2ObjectOpenHashMap<>();
        return OneShots.<PathfinderMob>of(Set.of(memoryToValidate, memoryToAcquire), (level, body, timestamp) -> {
            var brain = body.getBrain();
            if (brain.hasMemoryValue(memoryToAcquire) || brain.hasMemoryValue(memoryToValidate)) {
                return false;
            }
            if (onlyIfAdult && body.isBaby()) {
                return false;
            }
            RandomSource random = level.getRandom();
            if (nextScheduledStart[0] == 0L) {
                nextScheduledStart[0] = level.getGameTime() + random.nextInt(RATE);
                return false;
            }
            if (level.getGameTime() < nextScheduledStart[0]) {
                return false;
            }
            nextScheduledStart[0] = timestamp + RATE + random.nextInt(RATE);
            PoiManager poiManager = level.getPoiManager();
            batchCache.long2ObjectEntrySet().removeIf(entry -> !entry.getValue().isStillValid(timestamp));
            Predicate<BlockPos> cacheTest = pos -> {
                JitteredLinearRetry retry = batchCache.get(pos.asLong());
                if (retry == null) {
                    return true;
                }
                if (!retry.shouldRetry(timestamp)) {
                    return false;
                }
                retry.markAttempt(timestamp);
                return true;
            };
            Set<Pair<Holder<PoiType>, BlockPos>> candidates = poiManager
                .findAllClosestFirstWithType(poiType, cacheTest, body.blockPosition(), SCAN_RANGE, PoiManager.Occupancy.HAS_SPACE)
                .limit(BATCH_SIZE).filter(pair -> validPoi.test(level, pair.getSecond())).collect(Collectors.toSet());
            Path path = findPathToPois(body, candidates);
            if (path != null && path.canReach()) {
                BlockPos target = path.getTarget();
                poiManager.getType(target).ifPresent(type -> {
                    poiManager.take(poiType, (held, pos) -> pos.equals(target), target, 1);
                    brain.setMemory(memoryToAcquire, GlobalPos.of(level.dimension(), target));
                    onPoiAcquisitionEvent.ifPresent(event -> level.broadcastEntityEvent(body, event));
                    batchCache.clear();
                });
            } else {
                for (Pair<Holder<PoiType>, BlockPos> pair : candidates) {
                    batchCache.computeIfAbsent(pair.getSecond().asLong(), key -> new JitteredLinearRetry(random, timestamp));
                }
            }
            return true;
        });
    }

    public static Path findPathToPois(Mob body, Set<Pair<Holder<PoiType>, BlockPos>> pois) {
        if (pois.isEmpty()) {
            return null;
        }
        Set<BlockPos> targets = new HashSet<>();
        int maxRange = 1;
        for (Pair<Holder<PoiType>, BlockPos> pair : pois) {
            maxRange = Math.max(maxRange, pair.getFirst().value().validRange());
            targets.add(pair.getSecond());
        }
        return body.getNavigation().createPath(targets, maxRange);
    }

    static class JitteredLinearRetry {
        private static final int MIN_INTERVAL_INCREASE = 40;
        private static final int MAX_RETRY_PATHFINDING_INTERVAL = 400;
        private final RandomSource random;
        private long previousAttemptTimestamp;
        private long nextScheduledAttemptTimestamp;
        private int currentDelay;

        public JitteredLinearRetry(RandomSource random, long firstAttemptTimestamp) {
            this.random = random;
            markAttempt(firstAttemptTimestamp);
        }

        public void markAttempt(long timestamp) {
            previousAttemptTimestamp = timestamp;
            currentDelay = Math.min(currentDelay + random.nextInt(MIN_INTERVAL_INCREASE) + MIN_INTERVAL_INCREASE, MAX_RETRY_PATHFINDING_INTERVAL);
            nextScheduledAttemptTimestamp = timestamp + currentDelay;
        }

        public boolean isStillValid(long timestamp) {
            return timestamp - previousAttemptTimestamp < MAX_RETRY_PATHFINDING_INTERVAL;
        }

        public boolean shouldRetry(long timestamp) {
            return timestamp >= nextScheduledAttemptTimestamp;
        }

        public String toString() {
            return "RetryMarker{, previousAttemptAt=" + previousAttemptTimestamp + ", nextScheduledAttemptAt=" + nextScheduledAttemptTimestamp + ", currentDelay=" + currentDelay + "}";
        }
    }
}
