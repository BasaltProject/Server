package org.basaltmc.bukkit;

import java.util.List;
import java.util.Set;
import java.util.UUID;
import org.bukkit.Location;
import org.bukkit.Raid;
import org.bukkit.boss.BossBar;
import org.bukkit.entity.Raider;
import org.bukkit.persistence.PersistentDataContainer;

/** A raid as it stood when it ended, which is all the server tells about it. */
final class BasaltEndedRaid implements Raid {
    private final Location center;
    private final RaidStatus status;
    private final int waves;
    private int badOmen;
    private int totalWaves;

    BasaltEndedRaid(Location center, RaidStatus status, int waves) {
        this.center = center;
        this.status = status;
        this.waves = waves;
        this.totalWaves = waves;
    }

    @Override
    public boolean isStarted() {
        return true;
    }

    @Override
    public long getActiveTicks() {
        return 0;
    }

    @Override
    public int getBadOmenLevel() {
        return badOmen;
    }

    @Override
    public void setBadOmenLevel(int level) {
        badOmen = level;
    }

    @Override
    public Location getLocation() {
        return center.clone();
    }

    @Override
    public RaidStatus getStatus() {
        return status;
    }

    @Override
    public int getSpawnedGroups() {
        return waves;
    }

    @Override
    public int getTotalGroups() {
        return waves;
    }

    @Override
    public int getTotalWaves() {
        return totalWaves;
    }

    @Override
    public void setTotalWaves(int total) {
        totalWaves = total;
    }

    @Override
    public float getTotalHealth() {
        return 0f;
    }

    @Override
    public Set<UUID> getHeroes() {
        return Set.of();
    }

    @Override
    public List<Raider> getRaiders() {
        return List.of();
    }

    @Override
    public int getId() {
        return 0;
    }

    @Override
    public BossBar getBossBar() {
        throw Unsupported.hit("Raid#getBossBar() after the raid ended");
    }

    @Override
    public PersistentDataContainer getPersistentDataContainer() {
        throw Unsupported.hit("Raid#getPersistentDataContainer()");
    }
}
