// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.debugchart;

import net.minecraft.util.debug.DebugSubscription;
import net.minecraft.util.debug.DebugSubscriptions;

public enum RemoteDebugSampleType {
    TICK_TIME;

    public DebugSubscription<?> subscription() {
        return DebugSubscriptions.DEDICATED_SERVER_TICK_TIME;
    }
}
