// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package org.basaltmc.bukkit.facade;

import com.google.common.collect.ImmutableMap;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;

public final class DatafixTables {
    private DatafixTables() {}

    /** Reads `key=value` pairs separated by spaces, in order. */
    public static Map<String, String> pairs(String packed) {
        Map<String, String> out = new LinkedHashMap<>();
        for (String pair : packed.split(" ")) {
            int split = pair.indexOf('=');
            if (split > 0) {
                out.put(pair.substring(0, split), pair.substring(split + 1));
            }
        }
        return Collections.unmodifiableMap(out);
    }

    public static ImmutableMap<String, String> immutable(String packed) {
        return ImmutableMap.copyOf(pairs(packed));
    }
}
