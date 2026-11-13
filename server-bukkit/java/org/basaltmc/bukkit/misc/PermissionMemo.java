package org.basaltmc.bukkit.misc;

import java.util.concurrent.ConcurrentHashMap;
import org.basaltmc.bukkit.Natives;

/** Answers of the native permission check, kept until the next event, task or mutation. */
public final class PermissionMemo {
    private static final ConcurrentHashMap<String, Boolean> kAnswers = new ConcurrentHashMap<>();
    private static volatile long kTick = -1;

    private PermissionMemo() {}

    public static boolean may(String uuid, String node) {
        long now = org.basaltmc.bukkit.BasaltPlayer.cacheSerial();
        if (now != kTick) {
            kAnswers.clear();
            kTick = now;
        }
        return kAnswers.computeIfAbsent(uuid + "|" + node, key -> Natives.playerMay(uuid, node));
    }

    public static void forget() {
        kAnswers.clear();
    }
}
