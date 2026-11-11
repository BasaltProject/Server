// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import com.mojang.authlib.GameProfile;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.UUID;
import java.util.function.BooleanSupplier;
import java.util.regex.Pattern;
import java.util.stream.Stream;
import net.minecraft.CrashReport;
import net.minecraft.SystemReport;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.Registries;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.WorldStem;
import net.minecraft.server.packs.repository.PackRepository;
import net.minecraft.server.permissions.LevelBasedPermissionSet;
import net.minecraft.server.permissions.PermissionSet;
import net.minecraft.server.players.NameAndId;
import net.minecraft.server.players.ProfileResolver;
import net.minecraft.server.players.UserNameToIdResolver;
import net.minecraft.util.debugchart.SampleLogger;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.LevelStorageSource;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class GameTestServer extends MinecraftServer {
    private static final Logger LOGGER = LoggerFactory.getLogger(GameTestServer.class);
    private static final int kGridOriginY = -59;
    private final Optional<String> testSelection;
    private final boolean verify;
    private final int repeatCount;
    private MultipleTestTracker tracker;
    private boolean started;
    private final java.util.concurrent.atomic.AtomicBoolean finished = new java.util.concurrent.atomic.AtomicBoolean();

    public static GameTestServer create(Thread serverThread, LevelStorageSource.LevelStorageAccess levelStorageSource, PackRepository packRepository, Optional<String> testSelection, boolean verify, int repeatCount) {
        return new GameTestServer(serverThread, levelStorageSource, packRepository, null, testSelection, verify, repeatCount);
    }

    private GameTestServer(Thread serverThread, LevelStorageSource.LevelStorageAccess levelStorageSource, PackRepository packRepository, WorldStem worldStem, Optional<String> testSelection, boolean verify, int repeatCount) {
        super();
        this.testSelection = testSelection;
        this.verify = verify;
        this.repeatCount = Math.max(1, repeatCount);
    }

    public void start() {
        initServer();
    }

    @Override
    protected boolean initServer() {
        if (started) {
            return true;
        }
        started = true;
        List<Holder.Reference<GameTestInstance>> tests = getTestsForSelection(registryAccess(), testSelection.orElse("*")).toList();
        List<GameTestInfo> infos = new ArrayList<>();
        for (int round = 0; round < repeatCount; round++) {
            for (Holder.Reference<GameTestInstance> test : tests) {
                for (GameTestBatch batch : GameTestBatchFactory.divideIntoBatches(List.of(test), GameTestBatchFactory.DIRECT, this)) {
                    infos.addAll(batch.gameTestInfos());
                }
            }
        }
        LOGGER.info("Running {} game test(s){}", infos.size(), verify ? " in verify mode" : "");
        tracker = new MultipleTestTracker(infos);
        GameTestRunner runner = GameTestRunner.Builder.fromInfo(infos, this)
            .newStructureSpawner(new StructureGridSpawner(dimension -> origin(dimension), GameTestRunner.DEFAULT_TESTS_PER_ROW, true))
            .build();
        runner.start();
        addTickable(this::driveTests);
        return true;
    }

    private static BlockPos origin(net.minecraft.resources.ResourceKey<Level> dimension) {
        return new BlockPos(0, kGridOriginY, 0);
    }

    private void driveTests() {
        GameTestTicker.SINGLETON.tick();
        if (tracker != null && tracker.isDone() && finished.compareAndSet(false, true)) {
            GlobalTestReporter.finish();
            LOGGER.info("Game tests finished: {} passed, {} required failed, {} optional failed",
                tracker.getDoneCount() - tracker.getFailedRequiredCount() - tracker.getFailedOptionalCount(),
                tracker.getFailedRequiredCount(), tracker.getFailedOptionalCount());
            halt(false);
        }
    }

    public boolean hasFailedRequired() {
        return tracker != null && tracker.hasFailedRequired();
    }

    public static Stream<Holder.Reference<GameTestInstance>> getTestsForSelection(RegistryAccess registries, String selection) {
        Pattern glob = Pattern.compile(Stream.of(selection.split("\\*", -1)).map(Pattern::quote).collect(java.util.stream.Collectors.joining(".*")));
        return registries.lookupOrThrow(Registries.TEST_INSTANCE).listElements()
            .filter(holder -> glob.matcher(holder.key().identifier().toString()).matches()
                || glob.matcher(holder.key().identifier().getPath()).matches())
            .filter(holder -> !holder.value().manualOnly() || !selection.contains("*"));
    }

    @Override
    protected void tickServer(BooleanSupplier haveTime) {
        super.tickServer(haveTime);
    }

    @Override
    protected SampleLogger getTickTimeLogger() {
        return new SampleLogger() {
            @Override
            public void logFullSample(long[] sample) {
            }

            @Override
            public void logSample(long sample) {
            }

            @Override
            public void logPartialSample(long sample, int dimension) {
            }
        };
    }

    @Override
    public boolean isTickTimeLoggingEnabled() {
        return false;
    }

    @Override
    protected void waitUntilNextTick() {
        runAllTasks();
    }

    @Override
    public SystemReport fillServerSystemReport(SystemReport systemReport) {
        systemReport.setDetail("Type", "Game test server");
        return systemReport;
    }

    @Override
    protected void onServerExit() {
        super.onServerExit();
        GlobalTestReporter.finish();
    }

    @Override
    protected void onServerCrash(CrashReport report) {
        LOGGER.error("Game test server crashed: {}", report.getTitle());
        super.onServerCrash(report);
    }

    @Override
    public boolean isHardcore() {
        return false;
    }

    @Override
    public LevelBasedPermissionSet operatorUserPermissions() {
        return LevelBasedPermissionSet.OWNER;
    }

    @Override
    public PermissionSet getFunctionCompilationPermissions() {
        return LevelBasedPermissionSet.OWNER;
    }

    @Override
    public boolean shouldRconBroadcast() {
        return false;
    }

    @Override
    public boolean isDedicatedServer() {
        return false;
    }

    @Override
    public int getRateLimitPacketsPerSecond() {
        return 0;
    }

    @Override
    public int getCommandSpamThresholdSeconds() {
        return 0;
    }

    @Override
    public int getChatSpamThresholdSeconds() {
        return 0;
    }

    @Override
    public boolean useNativeTransport() {
        return false;
    }

    @Override
    public boolean isPublished() {
        return false;
    }

    @Override
    public boolean shouldInformAdmins() {
        return false;
    }

    @Override
    public boolean isSingleplayerOwner(NameAndId nameAndId) {
        return false;
    }

    @Override
    public int getMaxPlayers() {
        return 1;
    }

    static class MockUserNameToIdResolver implements UserNameToIdResolver {
        private final java.util.Map<UUID, NameAndId> byId = new java.util.concurrent.ConcurrentHashMap<>();

        @Override
        public void add(NameAndId nameAndId) {
            byId.put(nameAndId.id(), nameAndId);
        }

        @Override
        public Optional<NameAndId> get(String name) {
            return byId.values().stream().filter(entry -> entry.name().equalsIgnoreCase(name)).findFirst();
        }

        @Override
        public Optional<NameAndId> get(UUID id) {
            return Optional.ofNullable(byId.get(id));
        }

        @Override
        public void resolveOfflineUsers(boolean value) {
        }

        @Override
        public void save() {
        }
    }

    static class MockProfileResolver implements ProfileResolver {
        @Override
        public Optional<GameProfile> fetchByName(String name) {
            return Optional.empty();
        }

        @Override
        public Optional<GameProfile> fetchById(UUID id) {
            return Optional.empty();
        }
    }
}
