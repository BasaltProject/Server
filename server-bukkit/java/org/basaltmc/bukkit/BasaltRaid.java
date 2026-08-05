package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.UUID;
import org.bukkit.Location;
import org.bukkit.Raid;
import org.bukkit.entity.Raider;
import org.bukkit.persistence.PersistentDataContainer;

final class BasaltRaid implements Raid {
    private final BasaltWorld world;
    private final int id;
    private final boolean started;
    private final long activeTicks;
    private int omenLevel;
    private final Location center;
    private final RaidStatus status;
    private final int spawnedGroups;
    private int totalGroups;
    private final float totalHealth;
    private final Set<UUID> heroes = new LinkedHashSet<>();
    private final List<Integer> raiders = new ArrayList<>();
    private PersistentDataContainer container;

    BasaltRaid(BasaltWorld world, String row) {
        this.world = world;
        String[] parts = row.split("\\|", -1);
        this.id = Integer.parseInt(parts[0]);
        this.started = parts[1].equals("1");
        this.activeTicks = Long.parseLong(parts[2]);
        this.omenLevel = Integer.parseInt(parts[3]);
        String[] at = parts[4].split(",");
        this.center = new Location(world, Integer.parseInt(at[0]), Integer.parseInt(at[1]), Integer.parseInt(at[2]));
        this.status = RaidStatus.valueOf(parts[5].toUpperCase(java.util.Locale.ROOT));
        this.spawnedGroups = Integer.parseInt(parts[6]);
        this.totalGroups = Integer.parseInt(parts[7]);
        this.totalHealth = Float.parseFloat(parts[8]);
        for (String hero : parts[9].isEmpty() ? new String[0] : parts[9].split(",")) {
            heroes.add(UUID.fromString(hero));
        }
        for (String raider : parts[10].isEmpty() ? new String[0] : parts[10].split(",")) {
            raiders.add(Integer.parseInt(raider));
        }
    }

    @Override
    public boolean isStarted() {
        return started;
    }

    @Override
    public long getActiveTicks() {
        return activeTicks;
    }

    @Override
    public int getBadOmenLevel() {
        return omenLevel;
    }

    @Override
    public void setBadOmenLevel(int level) {
        if (level < 0 || level > 5) {
            throw new IllegalArgumentException("Bad omen level must be between 0 and 5");
        }
        omenLevel = level;
        WorldEnvNatives.setRaid(world.getName(), id, omenLevel, totalGroups);
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
        return spawnedGroups;
    }

    @Override
    public int getTotalGroups() {
        return totalGroups + (omenLevel > 1 ? 1 : 0);
    }

    @Override
    public int getTotalWaves() {
        return totalGroups;
    }

    @Override
    public void setTotalWaves(int waves) {
        totalGroups = waves;
        WorldEnvNatives.setRaid(world.getName(), id, omenLevel, totalGroups);
    }

    @Override
    public float getTotalHealth() {
        return totalHealth;
    }

    @Override
    public Set<UUID> getHeroes() {
        return new LinkedHashSet<>(heroes);
    }

    @Override
    public List<Raider> getRaiders() {
        List<Raider> out = new ArrayList<>();
        for (int entity : raiders) {
            if (world.entity(entity) instanceof Raider raider) {
                out.add(raider);
            }
        }
        return out;
    }

    @Override
    public int getId() {
        return id;
    }

    @Override
    public org.bukkit.boss.BossBar getBossBar() {
        return new org.basaltmc.bukkit.misc.BasaltRaidBar(world.getName(), id);
    }

    @Override
    public PersistentDataContainer getPersistentDataContainer() {
        if (container == null) {
            container = new org.bukkit.craftbukkit.persistence.CraftPersistentDataContainer(
                new org.basaltmc.bukkit.persistence.BytesPdcStore(() -> WorldEnvNatives.raidValues(world.getName(), id),
                    bytes -> WorldEnvNatives.setRaidValues(world.getName(), id, bytes)));
        }
        return container;
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltRaid raid && raid.id == id && raid.world.equals(world);
    }

    @Override
    public int hashCode() {
        return id * 31 + world.hashCode();
    }
}
