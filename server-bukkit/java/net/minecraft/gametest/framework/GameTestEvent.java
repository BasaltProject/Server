// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

public class GameTestEvent {
    public final Long expectedDelay;
    public final Long minimumDelay;
    public final Runnable assertion;

    private GameTestEvent(Long expectedDelay, Long minimumDelay, Runnable assertion) {
        this.expectedDelay = expectedDelay;
        this.minimumDelay = minimumDelay;
        this.assertion = assertion;
    }

    public static GameTestEvent create(Runnable runnable) {
        return new GameTestEvent(null, null, runnable);
    }

    public static GameTestEvent create(long expectedTick, Runnable runnable) {
        return new GameTestEvent(expectedTick, null, runnable);
    }

    public static GameTestEvent createWithMinimumDelay(long minimumDelay, Runnable runnable) {
        return new GameTestEvent(null, minimumDelay, runnable);
    }
}
