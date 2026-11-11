// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import java.util.Collection;
import java.util.concurrent.CopyOnWriteArrayList;

public class GameTestTicker {
    public static final GameTestTicker SINGLETON = new GameTestTicker();
    private final Collection<GameTestInfo> testInfos = new CopyOnWriteArrayList<>();
    private GameTestRunner runner;
    private State state = State.IDLE;

    private GameTestTicker() {
    }

    public void add(GameTestInfo testInfo) {
        testInfos.add(testInfo);
    }

    public void clear() {
        if (state != State.IDLE) {
            state = State.HALTING;
            return;
        }
        testInfos.clear();
        if (runner != null) {
            runner.stop();
            runner = null;
        }
    }

    public void setRunner(GameTestRunner runner) {
        this.runner = runner;
    }

    public void tick() {
        if (runner == null) {
            return;
        }
        state = State.RUNNING;
        for (GameTestInfo info : testInfos) {
            info.tick(runner);
        }
        testInfos.removeIf(GameTestInfo::isDone);
        State finished = state;
        state = State.IDLE;
        if (finished == State.HALTING) {
            clear();
        }
    }

    private enum State {
        IDLE, RUNNING, HALTING
    }
}
