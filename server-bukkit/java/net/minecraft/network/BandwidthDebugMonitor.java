// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import java.util.concurrent.atomic.AtomicInteger;
import net.minecraft.util.debugchart.LocalSampleLogger;

public class BandwidthDebugMonitor {
    private final AtomicInteger received = new AtomicInteger();
    private final LocalSampleLogger logger;

    public BandwidthDebugMonitor(LocalSampleLogger bandwidthLogger) {
        this.logger = bandwidthLogger;
    }

    public void onReceive(int bytes) {
        received.addAndGet(bytes);
    }

    public void tick() {
        logger.logSample(received.getAndSet(0));
    }
}
