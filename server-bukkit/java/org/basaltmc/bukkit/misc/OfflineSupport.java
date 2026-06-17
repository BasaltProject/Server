package org.basaltmc.bukkit.misc;

import com.destroystokyo.paper.profile.PlayerProfile;
import java.time.Duration;
import java.time.Instant;
import java.util.Date;
import java.util.UUID;
import org.basaltmc.bukkit.MiscNatives;
import org.bukkit.BanEntry;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.World;

/** What OfflinePlayer asks of the player database, shared by the offline objects and the online ones. */
public final class OfflineSupport {
    private OfflineSupport() {}

    private static final String kDefaultSource = "(Unknown)";

    public static boolean isBanned(UUID id) {
        return MiscNatives.banOf(id.toString(), false) != null;
    }

    @SuppressWarnings("unchecked")
    public static <E extends BanEntry<? super PlayerProfile>> E ban(UUID id, String name, String reason, Date expires,
            String source) {
        String shown = name == null ? "" : name;
        MiscNatives.ban(id.toString(), shown, reason == null ? "" : reason, source == null ? kDefaultSource : source,
            System.currentTimeMillis(), expires == null ? -1 : expires.getTime(), false);
        return (E) new BasaltBanList.Profiles().getBanEntry((PlayerProfile) new BasaltPlayerProfile(id, name));
    }

    public static <E extends BanEntry<? super PlayerProfile>> E ban(UUID id, String name, String reason,
            Instant expires, String source) {
        return ban(id, name, reason, expires == null ? null : Date.from(expires), source);
    }

    public static <E extends BanEntry<? super PlayerProfile>> E ban(UUID id, String name, String reason,
            Duration duration, String source) {
        return ban(id, name, reason, duration == null ? null : Date.from(Instant.now().plus(duration)), source);
    }

    public static boolean isWhitelisted(UUID id) {
        return MiscNatives.isWhitelisted(id.toString());
    }

    public static void setWhitelisted(UUID id, String name, boolean value) {
        MiscNatives.setWhitelisted(id.toString(), name == null ? "" : name, value);
    }

    public static boolean isOp(UUID id) {
        return MiscNatives.isOperator(id.toString());
    }

    public static void setOp(UUID id, boolean value) {
        MiscNatives.setOperator(id.toString(), value);
    }

    public static long firstPlayed(UUID id) {
        Access.Known known = Access.byUuid(id);
        if (known != null && known.firstSeen() > 0) {
            return known.firstSeen();
        }
        return Bukkit.getPlayer(id) != null ? System.currentTimeMillis() : 0;
    }

    public static long lastSeen(UUID id) {
        Access.Known known = Access.byUuid(id);
        return known == null ? 0 : known.lastSeen();
    }

    public static boolean hasPlayedBefore(UUID id) {
        return Access.byUuid(id) != null || Bukkit.getPlayer(id) != null;
    }

    public static Location respawnLocation(UUID id) {
        String[] point = MiscNatives.spawnPoint(id.toString());
        World world = point == null ? null : Bukkit.getWorld(point[0]);
        return world == null ? null : new Location(world, Double.parseDouble(point[1]), Double.parseDouble(point[2]),
            Double.parseDouble(point[3]));
    }

    public static Location lastLocation(UUID id) {
        Access.Known known = Access.byUuid(id);
        if (known == null) {
            return null;
        }
        World world = Bukkit.getWorld(known.world());
        return world == null ? null : new Location(world, known.x(), known.y(), known.z());
    }
}
