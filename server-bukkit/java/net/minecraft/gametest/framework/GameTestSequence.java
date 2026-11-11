// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.function.Supplier;
import net.minecraft.network.chat.Component;

public class GameTestSequence {
    private final GameTestInfo parent;
    private final List<GameTestEvent> events = new ArrayList<>();
    private int lastTick;

    public GameTestSequence(GameTestInfo parent) {
        this.parent = parent;
        this.lastTick = parent.getTick();
    }

    private GameTestSequence add(GameTestEvent event) {
        events.add(event);
        return this;
    }

    public GameTestSequence thenWaitUntil(Runnable assertion) {
        return add(GameTestEvent.create(assertion));
    }

    public GameTestSequence thenWaitUntil(long expectedDelay, Runnable assertion) {
        return add(GameTestEvent.create(expectedDelay, assertion));
    }

    public GameTestSequence thenWaitAtLeast(long minimumDelay, Runnable assertion) {
        return add(GameTestEvent.createWithMinimumDelay(minimumDelay, assertion));
    }

    public GameTestSequence thenIdle(int delta) {
        return thenExecuteAfter(delta, () -> { });
    }

    public GameTestSequence thenExecute(Runnable assertion) {
        return add(GameTestEvent.create(() -> runGuarded(assertion)));
    }

    private void notYet() {
        throw new GameTestAssertException(Component.translatable("test.error.sequence.not_completed"), parent.getTick());
    }

    public GameTestSequence thenExecuteAfter(int delta, Runnable after) {
        return add(GameTestEvent.create(() -> {
            if (parent.getTick() < lastTick + delta) {
                notYet();
            }
            runGuarded(after);
        }));
    }

    public GameTestSequence thenExecuteFor(int delta, Runnable check) {
        return add(GameTestEvent.create(() -> {
            if (parent.getTick() < lastTick + delta) {
                runGuarded(check);
                notYet();
            }
        }));
    }

    public void thenSucceed() {
        add(GameTestEvent.create(parent::succeed));
    }

    public void thenFail(Supplier<GameTestException> e) {
        add(GameTestEvent.create(() -> parent.fail(e.get())));
    }

    public Condition thenTrigger() {
        Condition condition = new Condition();
        add(GameTestEvent.create(() -> condition.trigger(parent.getTick())));
        return condition;
    }

    public void tickAndContinue(int tick) {
        try {
            advance(tick);
        } catch (GameTestAssertException waiting) {
            // an unmet assertion just means the sequence keeps waiting
        }
    }

    public void tickAndFailIfNotComplete(int tick) {
        try {
            advance(tick);
        } catch (GameTestAssertException e) {
            parent.fail(e);
        }
    }

    private void runGuarded(Runnable assertion) {
        try {
            assertion.run();
        } catch (GameTestAssertException e) {
            parent.fail(e);
        }
    }

    private void advance(int tick) {
        Iterator<GameTestEvent> it = events.iterator();
        while (it.hasNext()) {
            GameTestEvent event = it.next();
            event.assertion.run();
            it.remove();
            int delay = tick - lastTick;
            int previous = lastTick;
            lastTick = tick;
            if (event.minimumDelay != null && event.minimumDelay > delay) {
                parent.fail(new GameTestAssertException(Component.translatable("test.error.sequence.minimum_tick", previous + event.minimumDelay), tick));
                return;
            }
            if (event.expectedDelay != null && event.expectedDelay != delay) {
                parent.fail(new GameTestAssertException(Component.translatable("test.error.sequence.invalid_tick", previous + event.expectedDelay), tick));
                return;
            }
        }
    }

    public class Condition {
        private static final int kNotTriggered = -1;
        private int triggerTime = kNotTriggered;

        public void trigger(int time) {
            if (triggerTime != kNotTriggered) {
                throw new IllegalStateException("Condition already triggered at " + triggerTime);
            }
            triggerTime = time;
        }

        public void assertTriggeredThisTick() {
            int tick = parent.getTick();
            if (triggerTime == tick) {
                return;
            }
            if (triggerTime == kNotTriggered) {
                throw new GameTestAssertException(Component.translatable("test.error.sequence.condition_not_triggered"), tick);
            }
            throw new GameTestAssertException(Component.translatable("test.error.sequence.condition_already_triggered", triggerTime), tick);
        }
    }
}
