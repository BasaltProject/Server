// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import it.unimi.dsi.fastutil.longs.LongArraySet;
import it.unimi.dsi.fastutil.longs.LongSet;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import java.util.stream.Collectors;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.ChunkPos;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class GameTestRunner {
    public static final int DEFAULT_TESTS_PER_ROW = 8;
    private static final Logger LOGGER = LoggerFactory.getLogger(GameTestRunner.class);
    private final MinecraftServer server;
    private final GameTestTicker testTicker;
    private final List<GameTestInfo> allTestInfos;
    private List<GameTestBatch> batches;
    private final List<GameTestBatchListener> batchListeners = new ArrayList<>();
    private final List<GameTestInfo> scheduledForRerun = new ArrayList<>();
    private final GameTestBatcher testBatcher;
    private boolean stopped = true;
    private TestEnvironmentDefinition.Activation<?> currentEnvironment;
    private final StructureSpawner existingStructureSpawner;
    private final StructureSpawner newStructureSpawner;
    private final boolean haltOnError;
    private final boolean clearBetweenBatches;

    protected GameTestRunner(GameTestBatcher batcher, Collection<GameTestBatch> batches, MinecraftServer server, GameTestTicker testTicker, StructureSpawner existingSpawner, StructureSpawner newSpawner, boolean haltOnError, boolean clearBetween) {
        this.server = server;
        this.testTicker = testTicker;
        this.testBatcher = batcher;
        this.existingStructureSpawner = existingSpawner;
        this.newStructureSpawner = newSpawner;
        this.batches = List.copyOf(batches);
        this.haltOnError = haltOnError;
        this.clearBetweenBatches = clearBetween;
        this.allTestInfos = this.batches.stream().flatMap(batch -> batch.gameTestInfos().stream()).collect(Collectors.toCollection(ArrayList::new));
        testTicker.setRunner(this);
        allTestInfos.forEach(info -> info.addListener(new ReportGameListener()));
    }

    public List<GameTestInfo> getTestInfos() {
        return allTestInfos;
    }

    public void start() {
        stopped = false;
        runBatch(0);
    }

    public void stop() {
        stopped = true;
        endCurrentEnvironment();
    }

    public void rerunTest(GameTestInfo info) {
        GameTestInfo fresh = info.copyReset();
        info.getListeners().forEach(listener -> listener.testAddedForRerun(info, fresh, this));
        allTestInfos.add(fresh);
        scheduledForRerun.add(fresh);
        if (stopped) {
            runScheduledRerunTests();
        }
    }

    private void releaseForcedChunks(GameTestInfo info) {
        LongSet forced = new LongArraySet(info.getLevel().getForceLoadedChunks());
        forced.forEach(pos -> info.getLevel().setChunkForced(ChunkPos.getX(pos), ChunkPos.getZ(pos), false));
    }

    private void runBatch(int batchIndex) {
        if (batchIndex >= batches.size()) {
            endCurrentEnvironment();
            runScheduledRerunTests();
            return;
        }
        if (batchIndex > 0 && clearBetweenBatches) {
            for (GameTestInfo info : batches.get(batchIndex - 1).gameTestInfos()) {
                var entity = info.getTestInstanceBlockEntity();
                StructureUtils.clearSpaceForStructure(entity.getTestBoundingBox(), info.getLevel());
                info.getLevel().destroyBlock(entity.getBlockPos(), false);
            }
        }
        GameTestBatch current = batches.get(batchIndex);
        existingStructureSpawner.onBatchStart(server);
        newStructureSpawner.onBatchStart(server);
        List<GameTestInfo> spawned = current.gameTestInfos().stream().map(this::spawn).flatMap(Optional::stream).toList();
        LOGGER.info("Running test environment '{}' batch {} ({} tests)...", current.environment().getRegisteredName(), current.index(), spawned.size());
        endCurrentEnvironment();
        currentEnvironment = TestEnvironmentDefinition.activate(current.environment().value(), server.getLevel(current.dimension()));
        batchListeners.forEach(listener -> listener.testBatchStarting(current));
        MultipleTestTracker tracker = new MultipleTestTracker();
        spawned.forEach(tracker::addTestToTrack);
        tracker.addListener(new GameTestListener() {
            private void completed(GameTestInfo info) {
                if (tracker.isDone()) {
                    batchListeners.forEach(listener -> listener.testBatchFinished(current));
                    releaseForcedChunks(info);
                    runBatch(batchIndex + 1);
                }
            }

            @Override
            public void testStructureLoaded(GameTestInfo info) {
            }

            @Override
            public void testPassed(GameTestInfo info, GameTestRunner runner) {
                info.getTestInstanceBlockEntity().removeBarriers();
                completed(info);
            }

            @Override
            public void testFailed(GameTestInfo info, GameTestRunner runner) {
                if (haltOnError) {
                    endCurrentEnvironment();
                    releaseForcedChunks(info);
                    GameTestTicker.SINGLETON.clear();
                } else {
                    completed(info);
                }
            }

            @Override
            public void testAddedForRerun(GameTestInfo original, GameTestInfo copy, GameTestRunner runner) {
            }
        });
        spawned.forEach(testTicker::add);
    }

    private void endCurrentEnvironment() {
        if (currentEnvironment != null) {
            currentEnvironment.teardown();
            currentEnvironment = null;
        }
    }

    private void runScheduledRerunTests() {
        if (scheduledForRerun.isEmpty()) {
            batches = List.of();
            stopped = true;
            return;
        }
        LOGGER.info("Starting re-run of tests: {}", scheduledForRerun.stream().map(info -> info.id().toString()).collect(Collectors.joining(", ")));
        batches = List.copyOf(testBatcher.batch(scheduledForRerun));
        scheduledForRerun.clear();
        stopped = false;
        runBatch(0);
    }

    public void addListener(GameTestBatchListener listener) {
        batchListeners.add(listener);
    }

    private Optional<GameTestInfo> spawn(GameTestInfo info) {
        return info.getTestBlockPos() == null ? newStructureSpawner.spawnStructure(info) : existingStructureSpawner.spawnStructure(info);
    }

    public interface StructureSpawner {
        StructureSpawner IN_PLACE = info -> Optional.ofNullable(info.prepareTestStructure()).map(ready -> ready.startExecution(1));
        StructureSpawner NOT_SET = info -> Optional.empty();

        Optional<GameTestInfo> spawnStructure(GameTestInfo testInfo);

        default void onBatchStart(MinecraftServer server) {
        }
    }

    public static class Builder {
        private final MinecraftServer server;
        private final GameTestTicker testTicker = GameTestTicker.SINGLETON;
        private GameTestBatcher batcher = GameTestBatchFactory.fromGameTestInfo();
        private StructureSpawner existingStructureSpawner = StructureSpawner.IN_PLACE;
        private StructureSpawner newStructureSpawner = StructureSpawner.NOT_SET;
        private final Collection<GameTestBatch> batches;
        private boolean haltOnError;
        private boolean clearBetweenBatches;

        private Builder(Collection<GameTestBatch> batches, MinecraftServer server) {
            this.batches = batches;
            this.server = server;
        }

        public static Builder fromBatches(Collection<GameTestBatch> batches, MinecraftServer server) {
            return new Builder(batches, server);
        }

        public static Builder fromInfo(Collection<GameTestInfo> tests, MinecraftServer server) {
            return fromBatches(GameTestBatchFactory.fromGameTestInfo().batch(tests), server);
        }

        public Builder haltOnError() {
            haltOnError = true;
            return this;
        }

        public Builder clearBetweenBatches() {
            clearBetweenBatches = true;
            return this;
        }

        public Builder newStructureSpawner(StructureSpawner structureSpawner) {
            newStructureSpawner = structureSpawner;
            return this;
        }

        public Builder existingStructureSpawner(StructureGridSpawner spawner) {
            existingStructureSpawner = spawner;
            return this;
        }

        public Builder batcher(GameTestBatcher batcher) {
            this.batcher = batcher;
            return this;
        }

        public GameTestRunner build() {
            return new GameTestRunner(batcher, batches, server, testTicker, existingStructureSpawner, newStructureSpawner, haltOnError, clearBetweenBatches);
        }
    }

    public interface GameTestBatcher {
        Collection<GameTestBatch> batch(Collection<GameTestInfo> infos);
    }
}
