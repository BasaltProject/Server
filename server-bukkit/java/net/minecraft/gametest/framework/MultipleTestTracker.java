// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.function.Consumer;

public class MultipleTestTracker {
    private static final char NOT_STARTED_TEST_CHAR = ' ';
    private static final char ONGOING_TEST_CHAR = '_';
    private static final char SUCCESSFUL_TEST_CHAR = '+';
    private static final char FAILED_OPTIONAL_TEST_CHAR = 'x';
    private static final char FAILED_REQUIRED_TEST_CHAR = 'X';
    private final Collection<GameTestInfo> tests = new ArrayList<>();
    private final Collection<GameTestListener> listeners = new ArrayList<>();

    public MultipleTestTracker() {}

    public MultipleTestTracker(Collection<GameTestInfo> tests) {
        this.tests.addAll(tests);
    }

    public void addTestToTrack(GameTestInfo testInfo) {
        tests.add(testInfo);
        listeners.forEach(testInfo::addListener);
    }

    public void addListener(GameTestListener listener) {
        listeners.add(listener);
        tests.forEach(test -> test.addListener(listener));
    }

    public void addFailureListener(Consumer<GameTestInfo> listener) {
        addListener(new GameTestListener() {
            @Override
            public void testStructureLoaded(GameTestInfo testInfo) {}

            @Override
            public void testPassed(GameTestInfo testInfo, GameTestRunner runner) {}

            @Override
            public void testFailed(GameTestInfo testInfo, GameTestRunner runner) {
                listener.accept(testInfo);
            }

            @Override
            public void testAddedForRerun(GameTestInfo original, GameTestInfo copy, GameTestRunner runner) {}
        });
    }

    public int getFailedRequiredCount() {
        return (int) tests.stream().filter(test -> test.hasFailed() && test.isRequired()).count();
    }

    public int getFailedOptionalCount() {
        return (int) tests.stream().filter(test -> test.hasFailed() && test.isOptional()).count();
    }

    public int getDoneCount() {
        return (int) tests.stream().filter(GameTestInfo::isDone).count();
    }

    public boolean hasFailedRequired() {
        return getFailedRequiredCount() > 0;
    }

    public boolean hasFailedOptional() {
        return getFailedOptionalCount() > 0;
    }

    public Collection<GameTestInfo> getFailedRequired() {
        List<GameTestInfo> failed = new ArrayList<>();
        for (GameTestInfo test : tests) {
            if (test.hasFailed() && test.isRequired()) {
                failed.add(test);
            }
        }
        return failed;
    }

    public Collection<GameTestInfo> getFailedOptional() {
        List<GameTestInfo> failed = new ArrayList<>();
        for (GameTestInfo test : tests) {
            if (test.hasFailed() && test.isOptional()) {
                failed.add(test);
            }
        }
        return failed;
    }

    public int getTotalCount() {
        return tests.size();
    }

    public boolean isDone() {
        return getDoneCount() == getTotalCount();
    }

    public String getProgressBar() {
        StringBuilder bar = new StringBuilder("[");
        for (GameTestInfo test : tests) {
            if (!test.hasStarted()) {
                bar.append(NOT_STARTED_TEST_CHAR);
            } else if (test.hasSucceeded()) {
                bar.append(SUCCESSFUL_TEST_CHAR);
            } else if (test.hasFailed()) {
                bar.append(test.isRequired() ? FAILED_REQUIRED_TEST_CHAR : FAILED_OPTIONAL_TEST_CHAR);
            } else {
                bar.append(ONGOING_TEST_CHAR);
            }
        }
        return bar.append(']').toString();
    }

    @Override
    public String toString() {
        return getProgressBar();
    }

    public void remove(GameTestInfo testInfo) {
        tests.remove(testInfo);
    }
}
