package org.basaltmc.bukkit.misc;

import java.lang.ref.Cleaner;
import java.util.LinkedHashSet;
import java.util.Set;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.MiscNatives;
import org.bukkit.Bukkit;
import org.bukkit.OfflinePlayer;
import org.bukkit.entity.Entity;
import org.bukkit.entity.EntityType;
import org.bukkit.scoreboard.Criteria;
import org.bukkit.scoreboard.DisplaySlot;
import org.bukkit.scoreboard.Objective;
import org.bukkit.scoreboard.RenderType;
import org.bukkit.scoreboard.Score;
import org.bukkit.scoreboard.Scoreboard;
import org.bukkit.scoreboard.Team;

public final class BasaltScoreboard implements Scoreboard {
    private static final Cleaner kCleaner = Cleaner.create();

    private final int handle;

    BasaltScoreboard(int handle) {
        this.handle = handle;
        if (handle != 0) {
            int held = handle;
            kCleaner.register(this, () -> MiscNatives.sbRelease(held));
        }
    }

    public int handle() {
        return handle;
    }

    static String entryOf(OfflinePlayer player) {
        if (player == null) {
            throw new IllegalArgumentException("OfflinePlayer cannot be null");
        }
        String name = player.getName();
        return name != null ? name : player.getUniqueId().toString();
    }

    static String entryOf(Entity entity) {
        if (entity == null) {
            throw new IllegalArgumentException("Entity cannot be null");
        }
        return entity.getType() == EntityType.PLAYER ? entity.getName() : entity.getUniqueId().toString();
    }

    static OfflinePlayer playerOf(String entry) {
        return Bukkit.getOfflinePlayer(entry);
    }

    @Override
    public Objective registerNewObjective(String name, String criteria, Component displayName, org.bukkit.scoreboard.RenderType renderType) {
        return register(name, criteria, displayName, renderType);
    }

    @Override
    public Objective registerNewObjective(String name, Criteria criteria, Component displayName, RenderType renderType) {
        if (criteria == null) {
            throw new IllegalArgumentException("Criteria cannot be null");
        }
        return register(name, criteria.getName(), displayName, renderType);
    }

    @Override
    public Objective registerNewObjective(String name, String criteria, String displayName, RenderType renderType) {
        return register(name, criteria, Json.fromLegacy(displayName), renderType);
    }

    private Objective register(String name, String criteria, Component displayName, RenderType renderType) {
        if (name == null) {
            throw new IllegalArgumentException("Objective name cannot be null");
        }
        if (criteria == null) {
            throw new IllegalArgumentException("Criteria cannot be null");
        }
        if (renderType == null) {
            throw new IllegalArgumentException("RenderType cannot be null");
        }
        if (name.length() > 32767) {
            throw new IllegalArgumentException("The name '" + name + "' is longer than the limit of 32767 characters");
        }
        String shown = Json.of(displayName == null ? Component.text(name) : displayName);
        String[] fields = {name, criteria, shown, renderType == RenderType.HEARTS ? "1" : "0", "1", "", "", "0"};
        if (MiscNatives.sbObjective(handle, name) != null) {
            throw new IllegalArgumentException("An objective of name '" + name + "' already exists");
        }
        if (!MiscNatives.sbAddObjective(handle, fields)) {
            throw new IllegalArgumentException("Invalid objective criteria: " + criteria);
        }
        return new BasaltObjective(this, name);
    }

    @Override
    public Objective getObjective(String name) {
        if (name == null) {
            throw new IllegalArgumentException("Objective name cannot be null");
        }
        return MiscNatives.sbObjective(handle, name) == null ? null : new BasaltObjective(this, name);
    }

    @Override
    public Set<Objective> getObjectivesByCriteria(String criteria) {
        Set<Objective> out = new LinkedHashSet<>();
        for (String name : MiscNatives.sbObjectives(handle)) {
            String[] fields = MiscNatives.sbObjective(handle, name);
            if (fields != null && fields[1].equals(criteria)) {
                out.add(new BasaltObjective(this, name));
            }
        }
        return out;
    }

    @Override
    public Set<Objective> getObjectivesByCriteria(Criteria criteria) {
        return getObjectivesByCriteria(criteria.getName());
    }

    @Override
    public Set<Objective> getObjectives() {
        Set<Objective> out = new LinkedHashSet<>();
        for (String name : MiscNatives.sbObjectives(handle)) {
            out.add(new BasaltObjective(this, name));
        }
        return out;
    }

    @Override
    public Objective getObjective(DisplaySlot slot) {
        if (slot == null) {
            throw new IllegalArgumentException("Display slot cannot be null");
        }
        String name = MiscNatives.sbDisplayed(handle, slot.ordinal());
        return name == null ? null : new BasaltObjective(this, name);
    }

    @Override
    public Set<Score> getScores(OfflinePlayer player) {
        return getScores(entryOf(player));
    }

    @Override
    public Set<Score> getScores(String entry) {
        if (entry == null) {
            throw new IllegalArgumentException("Entry cannot be null");
        }
        Set<Score> out = new LinkedHashSet<>();
        String[] flat = MiscNatives.sbScoresOf(handle, entry);
        for (int i = 0; i + 1 < flat.length; i += 2) {
            out.add(new BasaltScore(new BasaltObjective(this, flat[i]), entry));
        }
        return out;
    }

    @Override
    public void resetScores(OfflinePlayer player) {
        resetScores(entryOf(player));
    }

    @Override
    public void resetScores(String entry) {
        if (entry == null) {
            throw new IllegalArgumentException("Entry cannot be null");
        }
        MiscNatives.sbResetScore(handle, entry, null);
    }

    @Override
    public Team getPlayerTeam(OfflinePlayer player) {
        return getEntryTeam(entryOf(player));
    }

    @Override
    public Team getEntryTeam(String entry) {
        if (entry == null) {
            throw new IllegalArgumentException("Entry cannot be null");
        }
        String[] fields = MiscNatives.sbTeamOf(handle, entry);
        return fields == null ? null : new BasaltTeam(this, fields[0]);
    }

    @Override
    public Team getTeam(String teamName) {
        if (teamName == null) {
            throw new IllegalArgumentException("Team name cannot be null");
        }
        return MiscNatives.sbTeam(handle, teamName) == null ? null : new BasaltTeam(this, teamName);
    }

    @Override
    public Set<Team> getTeams() {
        Set<Team> out = new LinkedHashSet<>();
        for (String name : MiscNatives.sbTeams(handle)) {
            out.add(new BasaltTeam(this, name));
        }
        return out;
    }

    @Override
    public Team registerNewTeam(String name) {
        if (name == null) {
            throw new IllegalArgumentException("Team name cannot be null");
        }
        if (name.length() > 32767) {
            throw new IllegalArgumentException("Team name '" + name + "' is longer than the limit of 32767 characters");
        }
        if (!MiscNatives.sbAddTeam(handle, name)) {
            throw new IllegalArgumentException("Team name '" + name + "' is already in use");
        }
        return new BasaltTeam(this, name);
    }

    @Override
    public Set<OfflinePlayer> getPlayers() {
        Set<OfflinePlayer> out = new LinkedHashSet<>();
        for (String entry : MiscNatives.sbEntries(handle)) {
            out.add(playerOf(entry));
        }
        return out;
    }

    @Override
    public Set<String> getEntries() {
        return new LinkedHashSet<>(java.util.List.of(MiscNatives.sbEntries(handle)));
    }

    @Override
    public void clearSlot(DisplaySlot slot) {
        if (slot == null) {
            throw new IllegalArgumentException("Slot cannot be null");
        }
        MiscNatives.sbDisplay(handle, slot.ordinal(), null);
    }

    @Override
    public Set<Score> getScoresFor(Entity entity) {
        return getScores(entryOf(entity));
    }

    @Override
    public void resetScoresFor(Entity entity) {
        resetScores(entryOf(entity));
    }

    @Override
    public Team getEntityTeam(Entity entity) {
        return getEntryTeam(entryOf(entity));
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltScoreboard board && board.handle == handle;
    }

    @Override
    public int hashCode() {
        return handle;
    }
}
