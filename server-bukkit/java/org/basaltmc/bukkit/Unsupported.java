package org.basaltmc.bukkit;

import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

public final class Unsupported {
    private static final Set<String> reported = ConcurrentHashMap.newKeySet();

    private Unsupported() {}

    public static UnsupportedOperationException hit(String method) {
        if (reported.add(method)) {
            Natives.log(2, "Bukkit", method + " is not implemented by Basalt yet");
        }
        return new UnsupportedOperationException(method + " is not implemented by Basalt yet");
    }
}
