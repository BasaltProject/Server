package org.spigotmc;

import ca.spottedleaf.moonrise.common.util.TickThread;

/** The server's own tick loop is watched natively; plugins only poke this one to say a long task is alive. */
public class WatchdogThread extends TickThread {
    private static WatchdogThread instance = new WatchdogThread();
    private volatile long lastTick;

    protected WatchdogThread() {}

    public static void tick() {
        instance.lastTick = System.currentTimeMillis();
    }
}
