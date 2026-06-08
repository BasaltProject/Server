package org.spigotmc;

import ca.spottedleaf.moonrise.common.util.TickThread;
import org.basaltmc.bukkit.Unsupported;
public class WatchdogThread extends TickThread {
    protected WatchdogThread() {
    }
    public static void tick() {
        throw Unsupported.hit("org.spigotmc.WatchdogThread.tick");
    }
}
