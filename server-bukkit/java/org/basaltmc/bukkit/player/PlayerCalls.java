package org.basaltmc.bukkit.player;

import java.util.UUID;
import org.basaltmc.bukkit.PlayerNatives;
import org.basaltmc.bukkit.Unsupported;

public final class PlayerCalls {
    private PlayerCalls() {}

    public static Object get(UUID id, String name) {
        return PlayerNatives.get(id.toString(), name);
    }

    public static boolean flag(UUID id, String name) {
        return get(id, name) instanceof Boolean value && value;
    }

    public static long integer(UUID id, String name) {
        return get(id, name) instanceof Number value ? value.longValue() : 0L;
    }

    public static double real(UUID id, String name) {
        return get(id, name) instanceof Number value ? value.doubleValue() : 0.0;
    }

    public static String text(UUID id, String name) {
        return get(id, name) instanceof String value ? value : null;
    }

    public static boolean set(UUID id, String name, Object value) {
        return PlayerNatives.set(id.toString(), name, value);
    }

    public static Object call(UUID id, String operation, Object... arguments) {
        return PlayerNatives.call(id.toString(), operation, arguments);
    }

    public static boolean done(UUID id, String operation, Object... arguments) {
        return call(id, operation, arguments) instanceof Boolean value && value;
    }

    public static long count(UUID id, String operation, Object... arguments) {
        return call(id, operation, arguments) instanceof Number value ? value.longValue() : 0L;
    }

    public static UnsupportedOperationException unsupported(String what) {
        return Unsupported.hit(what);
    }
}
