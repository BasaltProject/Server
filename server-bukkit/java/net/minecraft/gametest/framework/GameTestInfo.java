// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.Vec3i;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.TestInstanceBlockEntity;
import net.minecraft.world.phys.AABB;

public class GameTestInfo {
    private final Holder.Reference<GameTestInstance> test;
    private BlockPos testBlockPos;
    private final ServerLevel level;
    private final Collection<GameTestListener> listeners = new ArrayList<>();
    private final int timeoutTicks;
    private final Collection<GameTestSequence> sequences = new CopyOnWriteArrayList<>();
    private final Map<Runnable, Long> runAtTickTime = new LinkedHashMap<>();
    private boolean placedStructure;
    private boolean chunksLoaded;
    private int tickCount;
    private boolean started;
    private final RetryOptions retryOptions;
    private long startedAtNanos;
    private long runNanos;
    private boolean done;
    private final Rotation extraRotation;
    private GameTestException error;
    private TestInstanceBlockEntity testInstanceBlockEntity;

    public GameTestInfo(Holder.Reference<GameTestInstance> test, Rotation extraRotation, ServerLevel level, RetryOptions retryOptions) {
        this.test = test;
        this.level = level;
        this.retryOptions = retryOptions;
        this.timeoutTicks = test.value().maxTicks();
        this.extraRotation = extraRotation;
    }

    public void setTestBlockPos(BlockPos testBlockPos) {
        this.testBlockPos = testBlockPos;
    }

    public GameTestInfo startExecution(int tickDelay) {
        tickCount = -(test.value().setupTicks() + tickDelay + 1);
        return this;
    }

    public void placeStructure() {
        if (placedStructure) {
            return;
        }
        TestInstanceBlockEntity entity = getTestInstanceBlockEntity();
        if (!entity.placeStructure()) {
            fail(Component.translatable("test.error.structure.failure", entity.getTestName().getString()));
        }
        placedStructure = true;
        entity.encaseStructure();
        var box = entity.getTestBoundingBox();
        level.getBlockTicks().clearArea(box);
        level.clearBlockEvents(box);
        listeners.forEach(listener -> listener.testStructureLoaded(this));
    }

    public void tick(GameTestRunner runner) {
        if (done) {
            return;
        }
        if (!placedStructure) {
            fail(Component.translatable("test.error.ticking_without_structure"));
        }
        if (testInstanceBlockEntity == null) {
            fail(Component.translatable("test.error.missing_block_entity"));
        }
        if (error != null) {
            finish();
        } else {
            if (!chunksLoaded && !testInstanceBlockEntity.getStructureBoundingBox().intersectingChunks().allMatch(level::areEntitiesActuallyLoadedAndTicking)) {
                return;
            }
            chunksLoaded = true;
        }
        tickInternal();
        if (done) {
            for (GameTestListener listener : listeners) {
                if (error != null) {
                    listener.testFailed(this, runner);
                } else {
                    listener.testPassed(this, runner);
                }
            }
        }
    }

    private void tickInternal() {
        tickCount++;
        if (tickCount < 0) {
            return;
        }
        if (!started) {
            startTest();
        }
        var due = runAtTickTime.entrySet().iterator();
        while (due.hasNext()) {
            Map.Entry<Runnable, Long> entry = due.next();
            if (entry.getValue() <= tickCount) {
                due.remove();
                runGuarded(entry.getKey());
            }
        }
        if (tickCount <= timeoutTicks) {
            sequences.forEach(sequence -> sequence.tickAndContinue(tickCount));
            return;
        }
        if (sequences.isEmpty()) {
            fail(new GameTestTimeoutException(Component.translatable("test.error.timeout.no_result", test.value().maxTicks())));
            return;
        }
        sequences.forEach(sequence -> sequence.tickAndFailIfNotComplete(tickCount));
        if (error == null) {
            fail(new GameTestTimeoutException(Component.translatable("test.error.timeout.no_sequences_finished", test.value().maxTicks())));
        }
    }

    private void runGuarded(Runnable action) {
        try {
            action.run();
        } catch (GameTestException e) {
            fail(e);
        } catch (Exception e) {
            fail(new UnknownGameTestException(e));
        }
    }

    private void startTest() {
        started = true;
        startedAtNanos = System.nanoTime();
        getTestInstanceBlockEntity().setRunning();
        runGuarded(() -> test.value().run(new GameTestHelper(this)));
    }

    public void setRunAtTickTime(long time, Runnable assertAtTickTime) {
        runAtTickTime.put(assertAtTickTime, time);
    }

    public Identifier id() {
        return test.key().identifier();
    }

    public BlockPos getTestBlockPos() {
        return testBlockPos;
    }

    public BlockPos getTestOrigin() {
        return testInstanceBlockEntity.getStartCorner();
    }

    public AABB getStructureBounds() {
        return getTestInstanceBlockEntity().getStructureBounds();
    }

    public TestInstanceBlockEntity getTestInstanceBlockEntity() {
        if (testInstanceBlockEntity != null) {
            return testInstanceBlockEntity;
        }
        if (testBlockPos == null) {
            throw new IllegalStateException("This GameTestInfo has no position");
        }
        if (level.getBlockEntity(testBlockPos) instanceof TestInstanceBlockEntity found) {
            testInstanceBlockEntity = found;
            return found;
        }
        throw new IllegalStateException("Could not find a test instance block entity at the given coordinate " + testBlockPos);
    }

    public ServerLevel getLevel() {
        return level;
    }

    public boolean hasSucceeded() {
        return done && error == null;
    }

    public boolean hasFailed() {
        return error != null;
    }

    public boolean hasStarted() {
        return started;
    }

    public boolean isDone() {
        return done;
    }

    public long getRunTime() {
        return (done || startedAtNanos == 0 ? runNanos : runNanos + System.nanoTime() - startedAtNanos) / 1_000_000L;
    }

    private void finish() {
        if (done) {
            return;
        }
        done = true;
        if (startedAtNanos != 0) {
            runNanos = System.nanoTime() - startedAtNanos;
        }
    }

    public void succeed() {
        if (error != null) {
            return;
        }
        finish();
        AABB bounds = getStructureBounds();
        for (Entity entity : level.getEntitiesOfClass(Entity.class, bounds.inflate(1.0), e -> !(e instanceof Player))) {
            entity.remove(Entity.RemovalReason.DISCARDED);
        }
    }

    public void fail(Component message) {
        fail(new GameTestAssertException(message, tickCount));
    }

    public void fail(GameTestException error) {
        this.error = error;
    }

    public GameTestException getError() {
        return error;
    }

    @Override
    public String toString() {
        return id().toString();
    }

    public void addListener(GameTestListener listener) {
        listeners.add(listener);
    }

    public GameTestInfo prepareTestStructure() {
        level.setBlockAndUpdate(testBlockPos, Blocks.TEST_INSTANCE_BLOCK.defaultBlockState());
        if (!(level.getBlockEntity(testBlockPos) instanceof TestInstanceBlockEntity entity)) {
            return null;
        }
        Vec3i size = TestInstanceBlockEntity.getStructureSize(level, test.key()).orElse(new Vec3i(1, 1, 1));
        entity.set(new TestInstanceBlockEntity.Data(Optional.of(test.key()), size, extraRotation, false, TestInstanceBlockEntity.Status.CLEARED, Optional.empty()));
        testInstanceBlockEntity = entity;
        placeStructure();
        return this;
    }

    public int getTick() {
        return tickCount;
    }

    public GameTestSequence createSequence() {
        GameTestSequence sequence = new GameTestSequence(this);
        sequences.add(sequence);
        return sequence;
    }

    public boolean isRequired() {
        return test.value().required();
    }

    public boolean isOptional() {
        return !isRequired();
    }

    public Identifier getStructure() {
        return test.value().structure();
    }

    public Rotation getRotation() {
        return Rotation.values()[(test.value().rotation().ordinal() + extraRotation.ordinal()) % 4];
    }

    public GameTestInstance getTest() {
        return test.value();
    }

    public Holder.Reference<GameTestInstance> getTestHolder() {
        return test;
    }

    public int getTimeoutTicks() {
        return timeoutTicks;
    }

    public boolean isFlaky() {
        return maxAttempts() > 1;
    }

    public int maxAttempts() {
        return test.value().maxAttempts();
    }

    public int requiredSuccesses() {
        return test.value().requiredSuccesses();
    }

    public RetryOptions retryOptions() {
        return retryOptions;
    }

    public Stream<GameTestListener> getListeners() {
        return listeners.stream();
    }

    public GameTestInfo copyReset() {
        GameTestInfo copy = new GameTestInfo(test, extraRotation, level, retryOptions);
        if (testBlockPos != null) {
            copy.setTestBlockPos(testBlockPos);
        }
        return copy;
    }
}
