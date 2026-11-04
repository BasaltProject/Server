// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

public class TickTask implements Runnable {
    private final int tick;
    private final Runnable runnable;

    public TickTask(int tick, Runnable runnable) {
        this.tick = tick;
        this.runnable = runnable;
    }

    public int getTick() {
        return tick;
    }

    public void run() {
        runnable.run();
    }
}
