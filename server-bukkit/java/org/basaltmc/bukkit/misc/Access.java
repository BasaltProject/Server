package org.basaltmc.bukkit.misc;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;
import org.basaltmc.bukkit.MiscNatives;

public final class Access {
    private Access() {}

    public record Known(UUID id, String name, String world, long firstSeen, long lastSeen, double x, double y, double z) {}

    public static UUID offlineUuid(String name) {
        return UUID.nameUUIDFromBytes(("OfflinePlayer:" + name).getBytes(StandardCharsets.UTF_8));
    }

    public static Known byUuid(UUID id) {
        return known(MiscNatives.knownPlayer(id.toString(), false), id.toString());
    }

    public static Known byName(String name) {
        String[] found = MiscNatives.knownPlayer(name, true);
        return found == null ? null : known(found, found[0]);
    }

    private static Known known(String[] strings, String uuid) {
        if (strings == null) {
            return null;
        }
        double[] numbers = MiscNatives.knownPlayerNumbers(uuid);
        if (numbers == null) {
            numbers = new double[5];
        }
        return new Known(UUID.fromString(strings[0]), strings[1], strings[2], (long) numbers[0], (long) numbers[1],
            numbers[2], numbers[3], numbers[4]);
    }

    public static List<Known> all() {
        return pairs(MiscNatives.knownPlayers());
    }

    public static List<Known> operators() {
        return pairs(MiscNatives.operators());
    }

    public static List<Known> whitelisted() {
        return pairs(MiscNatives.whitelisted());
    }

    private static List<Known> pairs(String[] flat) {
        List<Known> out = new ArrayList<>();
        for (int i = 0; flat != null && i + 1 < flat.length; i += 2) {
            out.add(new Known(UUID.fromString(flat[i]), flat[i + 1].isEmpty() ? null : flat[i + 1], "", 0, 0, 0, 0, 0));
        }
        return out;
    }
}
