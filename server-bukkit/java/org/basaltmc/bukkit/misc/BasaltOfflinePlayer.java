package org.basaltmc.bukkit.misc;

import com.destroystokyo.paper.profile.PlayerProfile;
import java.time.Duration;
import java.time.Instant;
import java.util.Date;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;
import org.basaltmc.bukkit.gen.OfflinePlayerBase;
import org.basaltmc.bukkit.persistence.PdcStores;
import org.bukkit.BanEntry;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.OfflinePlayer;
import org.bukkit.configuration.serialization.ConfigurationSerializable;
import org.bukkit.configuration.serialization.SerializableAs;
import org.bukkit.entity.Player;

/** A player who is not online, known by the database or only by a name. */
@SerializableAs("Player")
public final class BasaltOfflinePlayer extends OfflinePlayerBase implements ConfigurationSerializable {
    private final UUID id;
    private final String name;

    public BasaltOfflinePlayer(UUID id, String name) {
        this.id = id;
        this.name = name;
    }

    public static OfflinePlayer deserialize(Map<String, Object> args) {
        if (args.get("UUID") != null) {
            return Bukkit.getServer().getOfflinePlayer(UUID.fromString((String) args.get("UUID")));
        }
        return Bukkit.getServer().getOfflinePlayer((String) args.get("name"));
    }

    @Override
    public Map<String, Object> serialize() {
        Map<String, Object> result = new HashMap<>();
        result.put("UUID", id.toString());
        return result;
    }

    @Override
    public boolean isOnline() {
        return Bukkit.getPlayer(id) != null;
    }

    @Override
    public boolean isConnected() {
        return isOnline();
    }

    @Override
    public String getName() {
        if (name != null) {
            return name;
        }
        Access.Known known = Access.byUuid(id);
        return known == null ? null : known.name();
    }

    @Override
    public UUID getUniqueId() {
        return id;
    }

    @Override
    public PlayerProfile getPlayerProfile() {
        return new BasaltPlayerProfile(id, getName());
    }

    @Override
    public boolean isBanned() {
        return OfflineSupport.isBanned(id);
    }

    @Override
    public <E extends BanEntry<? super PlayerProfile>> E ban(String reason, Date expires, String source) {
        return OfflineSupport.ban(id, getName(), reason, expires, source);
    }

    @Override
    public <E extends BanEntry<? super PlayerProfile>> E ban(String reason, Instant expires, String source) {
        return OfflineSupport.ban(id, getName(), reason, expires, source);
    }

    @Override
    public <E extends BanEntry<? super PlayerProfile>> E ban(String reason, Duration duration, String source) {
        return OfflineSupport.ban(id, getName(), reason, duration, source);
    }

    @Override
    public boolean isWhitelisted() {
        return OfflineSupport.isWhitelisted(id);
    }

    @Override
    public void setWhitelisted(boolean value) {
        OfflineSupport.setWhitelisted(id, getName(), value);
    }

    @Override
    public Player getPlayer() {
        return Bukkit.getPlayer(id);
    }

    @Override
    public long getFirstPlayed() {
        return OfflineSupport.firstPlayed(id);
    }

    @Override
    public long getLastPlayed() {
        return OfflineSupport.lastSeen(id);
    }

    @Override
    public boolean hasPlayedBefore() {
        return OfflineSupport.hasPlayedBefore(id);
    }

    @Override
    public long getLastLogin() {
        return OfflineSupport.lastSeen(id);
    }

    @Override
    public long getLastSeen() {
        return OfflineSupport.lastSeen(id);
    }

    @Override
    public Location getRespawnLocation(boolean loadLocationAndValidate) {
        return OfflineSupport.respawnLocation(id);
    }

    @Override
    public Location getLocation() {
        return OfflineSupport.lastLocation(id);
    }

    @Override
    public org.bukkit.persistence.PersistentDataContainer getPersistentDataContainer() {
        return PdcStores.player(id.toString());
    }

    @Override
    public boolean isOp() {
        return OfflineSupport.isOp(id);
    }

    @Override
    public void setOp(boolean value) {
        OfflineSupport.setOp(id, value);
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof OfflinePlayer player && player.getUniqueId().equals(id);
    }

    @Override
    public int hashCode() {
        return id.hashCode();
    }

    @Override
    public String toString() {
        return "BasaltOfflinePlayer{id=" + id + ",name=" + getName() + "}";
    }
}
