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

    private long read(String key) {
        Player online = getPlayer();
        if (online != null) {
            return org.basaltmc.bukkit.player.PlayerCalls.count(id, "statistic", key);
        }
        return org.basaltmc.bukkit.RestNatives.offlineStatistic(id.toString(), key);
    }

    private void write(String key, long value) {
        Player online = getPlayer();
        if (online != null) {
            org.basaltmc.bukkit.player.PlayerCalls.call(id, "set_statistic", key, value);
        } else if (!org.basaltmc.bukkit.RestNatives.setOfflineStatistic(id.toString(), key, value)) {
            throw new IllegalStateException("The server has no record of " + id);
        }
    }

    private static void positive(int amount) {
        if (amount <= 0) {
            throw new IllegalArgumentException("Amount must be greater than 0");
        }
    }

    private void add(String key, int amount) {
        write(key, read(key) + amount);
    }

    @Override
    public int getStatistic(org.bukkit.Statistic statistic) {
        return (int) read(org.basaltmc.bukkit.player.PlayerStatistics.untyped(statistic));
    }

    @Override
    public int getStatistic(org.bukkit.Statistic statistic, org.bukkit.Material material) {
        return (int) read(org.basaltmc.bukkit.player.PlayerStatistics.of(statistic, material));
    }

    @Override
    public int getStatistic(org.bukkit.Statistic statistic, org.bukkit.entity.EntityType entity) {
        return (int) read(org.basaltmc.bukkit.player.PlayerStatistics.of(statistic, entity));
    }

    @Override
    public void setStatistic(org.bukkit.Statistic statistic, int value) {
        write(org.basaltmc.bukkit.player.PlayerStatistics.untyped(statistic), Math.max(value, 0));
    }

    @Override
    public void setStatistic(org.bukkit.Statistic statistic, org.bukkit.Material material, int value) {
        write(org.basaltmc.bukkit.player.PlayerStatistics.of(statistic, material), Math.max(value, 0));
    }

    @Override
    public void setStatistic(org.bukkit.Statistic statistic, org.bukkit.entity.EntityType entity, int value) {
        write(org.basaltmc.bukkit.player.PlayerStatistics.of(statistic, entity), Math.max(value, 0));
    }

    @Override
    public void incrementStatistic(org.bukkit.Statistic statistic) {
        incrementStatistic(statistic, 1);
    }

    @Override
    public void incrementStatistic(org.bukkit.Statistic statistic, int amount) {
        positive(amount);
        add(org.basaltmc.bukkit.player.PlayerStatistics.untyped(statistic), amount);
    }

    @Override
    public void incrementStatistic(org.bukkit.Statistic statistic, org.bukkit.Material material) {
        incrementStatistic(statistic, material, 1);
    }

    @Override
    public void incrementStatistic(org.bukkit.Statistic statistic, org.bukkit.Material material, int amount) {
        positive(amount);
        add(org.basaltmc.bukkit.player.PlayerStatistics.of(statistic, material), amount);
    }

    @Override
    public void incrementStatistic(org.bukkit.Statistic statistic, org.bukkit.entity.EntityType entity) {
        incrementStatistic(statistic, entity, 1);
    }

    @Override
    public void incrementStatistic(org.bukkit.Statistic statistic, org.bukkit.entity.EntityType entity, int amount) {
        positive(amount);
        add(org.basaltmc.bukkit.player.PlayerStatistics.of(statistic, entity), amount);
    }

    @Override
    public void decrementStatistic(org.bukkit.Statistic statistic) {
        decrementStatistic(statistic, 1);
    }

    @Override
    public void decrementStatistic(org.bukkit.Statistic statistic, int amount) {
        positive(amount);
        add(org.basaltmc.bukkit.player.PlayerStatistics.untyped(statistic), -amount);
    }

    @Override
    public void decrementStatistic(org.bukkit.Statistic statistic, org.bukkit.Material material) {
        decrementStatistic(statistic, material, 1);
    }

    @Override
    public void decrementStatistic(org.bukkit.Statistic statistic, org.bukkit.Material material, int amount) {
        positive(amount);
        add(org.basaltmc.bukkit.player.PlayerStatistics.of(statistic, material), -amount);
    }

    @Override
    public void decrementStatistic(org.bukkit.Statistic statistic, org.bukkit.entity.EntityType entity) {
        decrementStatistic(statistic, entity, 1);
    }

    @Override
    public void decrementStatistic(org.bukkit.Statistic statistic, org.bukkit.entity.EntityType entity, int amount) {
        positive(amount);
        add(org.basaltmc.bukkit.player.PlayerStatistics.of(statistic, entity), -amount);
    }

    @Override
    public Location getLastDeathLocation() {
        Player online = getPlayer();
        if (online != null) {
            return online.getLastDeathLocation();
        }
        String[] spot = org.basaltmc.bukkit.RestNatives.lastDeath(id.toString());
        if (spot == null) {
            return null;
        }
        org.bukkit.World world = Bukkit.getWorlds().stream().filter(w -> w.getKey().toString().equals(spot[0]))
            .findFirst().orElse(null);
        return world == null ? null : new Location(world, Integer.parseInt(spot[1]), Integer.parseInt(spot[2]),
            Integer.parseInt(spot[3]));
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
