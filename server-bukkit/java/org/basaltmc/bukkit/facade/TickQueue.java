package org.basaltmc.bukkit.facade;

import org.basaltmc.bukkit.BasaltServer;
import org.basaltmc.bukkit.Natives;
import org.bukkit.Bukkit;

public final class TickQueue {
    private TickQueue() {}

    public static void run(Runnable task) {
        if (Natives.onPluginThread()) {
            task.run();
        } else {
            ((BasaltServer) Bukkit.getServer()).tickTask(task);
        }
    }
}
