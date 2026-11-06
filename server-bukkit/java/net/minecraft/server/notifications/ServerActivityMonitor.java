// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.notifications;

public class ServerActivityMonitor {
    private final NotificationManager notificationManager;
    private final long minimumGapNanos;
    private long lastNotified;
    private boolean pending;

    public ServerActivityMonitor(NotificationManager notificationManager, int secondsBetweenNotifications) {
        this.notificationManager = notificationManager;
        this.minimumGapNanos = secondsBetweenNotifications * 1_000_000_000L;
    }

    public void tick() {
        long now = System.nanoTime();
        if (pending && now - lastNotified >= minimumGapNanos) {
            pending = false;
            lastNotified = now;
            notificationManager.serverActivityOccured();
        }
    }

    public void reportLoginActivity() {
        pending = true;
    }
}
