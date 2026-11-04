// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.scores;

import com.mojang.serialization.Codec;
import it.unimi.dsi.fastutil.objects.Object2IntMap;
import it.unimi.dsi.fastutil.objects.Object2IntOpenHashMap;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.EnumMap;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.numbers.NumberFormat;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.scores.criteria.ObjectiveCriteria;

public class Scoreboard {
    public static final String HIDDEN_SCORE_PREFIX = "#";
    private final Map<String, Objective> objectives = new LinkedHashMap<>();
    private final Map<ObjectiveCriteria, List<Objective>> byCriteria = new HashMap<>();
    private final Map<String, Map<Objective, Score>> scores = new LinkedHashMap<>();
    private final Map<DisplaySlot, Objective> displaySlots = new EnumMap<>(DisplaySlot.class);
    private final Map<String, PlayerTeam> teams = new LinkedHashMap<>();
    private final Map<String, PlayerTeam> teamByMember = new HashMap<>();

    public Objective getObjective(String name) {
        return objectives.get(name);
    }

    public Collection<Objective> getObjectives() {
        return Collections.unmodifiableCollection(objectives.values());
    }

    public Objective addObjective(String name, ObjectiveCriteria criteria, Component display,
        ObjectiveCriteria.RenderType render, boolean autoUpdate, NumberFormat format) {
        if (objectives.containsKey(name)) {
            throw new IllegalArgumentException("An objective with the name '" + name + "' already exists!");
        }
        Objective objective = new Objective(this, name, criteria, display, render, autoUpdate, format);
        objectives.put(name, objective);
        byCriteria.computeIfAbsent(criteria, k -> new ArrayList<>()).add(objective);
        onObjectiveAdded(objective);
        return objective;
    }

    public void removeObjective(Objective objective) {
        if (!objectives.remove(objective.getName(), objective)) {
            return;
        }
        List<Objective> sharing = byCriteria.get(objective.getCriteria());
        if (sharing != null) {
            sharing.remove(objective);
        }
        displaySlots.values().removeIf(objective::equals);
        for (Map<Objective, Score> perHolder : scores.values()) {
            perHolder.remove(objective);
        }
        onObjectiveRemoved(objective);
    }

    public void forAllObjectives(ObjectiveCriteria criteria, ScoreHolder name, Consumer<ScoreAccess> operation) {
        for (Objective objective : byCriteria.getOrDefault(criteria, List.of())) {
            operation.accept(getOrCreatePlayerScore(name, objective, true));
        }
    }

    public ScoreAccess getOrCreatePlayerScore(ScoreHolder holder, Objective objective) {
        return getOrCreatePlayerScore(holder, objective, false);
    }

    public ScoreAccess getOrCreatePlayerScore(ScoreHolder holder, Objective objective, boolean forceWritable) {
        boolean writable = forceWritable || !objective.getCriteria().isReadOnly();
        Score score = scores.computeIfAbsent(holder.getScoreboardName(), k -> new LinkedHashMap<>())
            .computeIfAbsent(objective, k -> {
                Score fresh = new Score();
                onScoreChanged(holder, objective, fresh);
                return fresh;
            });
        return new ScoreAccess() {
            @Override
            public int get() {
                return score.value();
            }

            @Override
            public void set(int value) {
                if (!writable) {
                    throw new IllegalStateException("Cannot modify read-only score");
                }
                boolean changed = score.value() != value;
                score.value(value);
                if (changed) {
                    onScoreChanged(holder, objective, score);
                }
            }

            @Override
            public boolean locked() {
                return score.isLocked();
            }

            @Override
            public void unlock() {
                setLocked(false);
            }

            @Override
            public void lock() {
                setLocked(true);
            }

            private void setLocked(boolean locked) {
                score.setLocked(locked);
                onScoreLockChanged(holder, objective);
            }

            @Override
            public Component display() {
                return score.display();
            }

            @Override
            public void display(Component display) {
                score.display(display);
                onScoreChanged(holder, objective, score);
            }

            @Override
            public void numberFormatOverride(NumberFormat numberFormat) {
                score.numberFormat(numberFormat);
                onScoreChanged(holder, objective, score);
            }
        };
    }

    public ReadOnlyScoreInfo getPlayerScoreInfo(ScoreHolder name, Objective objective) {
        Map<Objective, Score> perHolder = scores.get(name.getScoreboardName());
        return perHolder == null ? null : perHolder.get(objective);
    }

    public Collection<PlayerScoreEntry> listPlayerScores(Objective objective) {
        List<PlayerScoreEntry> entries = new ArrayList<>();
        scores.forEach((owner, perHolder) -> {
            Score score = perHolder.get(objective);
            if (score != null) {
                entries.add(new PlayerScoreEntry(owner, score.value(), score.display(), score.numberFormat()));
            }
        });
        return entries;
    }

    public Collection<String> getObjectiveNames() {
        return Collections.unmodifiableCollection(objectives.keySet());
    }

    public Collection<ScoreHolder> getTrackedPlayers() {
        return scores.keySet().stream().map(ScoreHolder::forNameOnly).toList();
    }

    public void resetAllPlayerScores(ScoreHolder player) {
        if (scores.remove(player.getScoreboardName()) != null) {
            onPlayerRemoved(player);
        }
    }

    public void resetSinglePlayerScore(ScoreHolder player, Objective objective) {
        Map<Objective, Score> perHolder = scores.get(player.getScoreboardName());
        if (perHolder != null && perHolder.remove(objective) != null) {
            if (perHolder.isEmpty()) {
                scores.remove(player.getScoreboardName());
            }
            onPlayerScoreRemoved(player, objective);
        }
    }

    public Object2IntMap<Objective> listPlayerScores(ScoreHolder player) {
        Object2IntMap<Objective> result = new Object2IntOpenHashMap<>();
        Map<Objective, Score> perHolder = scores.get(player.getScoreboardName());
        if (perHolder != null) {
            perHolder.forEach((objective, score) -> result.put(objective, score.value()));
        }
        return result;
    }

    public void setDisplayObjective(DisplaySlot slot, Objective objective) {
        if (objective == null) {
            displaySlots.remove(slot);
        } else {
            displaySlots.put(slot, objective);
        }
    }

    public Objective getDisplayObjective(DisplaySlot slot) {
        return displaySlots.get(slot);
    }

    public PlayerTeam getPlayerTeam(String name) {
        return teams.get(name);
    }

    public PlayerTeam addPlayerTeam(String name) {
        if (teams.containsKey(name)) {
            throw new IllegalArgumentException("A team with the name '" + name + "' already exists!");
        }
        PlayerTeam team = new PlayerTeam(this, name);
        teams.put(name, team);
        onTeamAdded(team);
        return team;
    }

    public void removePlayerTeam(PlayerTeam team) {
        teams.remove(team.getName());
        for (String member : team.getPlayers()) {
            teamByMember.remove(member);
        }
        onTeamRemoved(team);
    }

    public boolean addPlayerToTeam(String player, PlayerTeam team) {
        if (getPlayersTeam(player) != null) {
            removePlayerFromTeam(player);
        }
        teamByMember.put(player, team);
        boolean added = team.getPlayers().add(player);
        onTeamChanged(team);
        return added;
    }

    public boolean removePlayerFromTeam(String player) {
        PlayerTeam team = getPlayersTeam(player);
        if (team == null) {
            return false;
        }
        removePlayerFromTeam(player, team);
        return true;
    }

    public void removePlayerFromTeam(String player, PlayerTeam team) {
        if (getPlayersTeam(player) != team) {
            throw new IllegalStateException("Player is either on another team or not on any team. Cannot remove from team '"
                + team.getName() + "'.");
        }
        teamByMember.remove(player);
        team.getPlayers().remove(player);
        onTeamChanged(team);
    }

    public Collection<String> getTeamNames() {
        return Collections.unmodifiableCollection(teams.keySet());
    }

    public Collection<PlayerTeam> getPlayerTeams() {
        return Collections.unmodifiableCollection(teams.values());
    }

    public PlayerTeam getPlayersTeam(String name) {
        return teamByMember.get(name);
    }

    public void onObjectiveAdded(Objective objective) {
    }

    public void onObjectiveChanged(Objective objective) {
    }

    public void onObjectiveRemoved(Objective objective) {
    }

    protected void onScoreChanged(ScoreHolder owner, Objective objective, Score score) {
    }

    protected void onScoreLockChanged(ScoreHolder owner, Objective objective) {
    }

    public void onPlayerRemoved(ScoreHolder player) {
    }

    public void onPlayerScoreRemoved(ScoreHolder player, Objective objective) {
    }

    public void onTeamAdded(PlayerTeam team) {
    }

    public void onTeamChanged(PlayerTeam team) {
    }

    public void onTeamRemoved(PlayerTeam team) {
    }

    public void entityRemoved(Entity entity) {
        if (!(entity instanceof net.minecraft.world.entity.player.Player)
            && !entity.isAlive()) {
            resetAllPlayerScores(entity);
            removePlayerFromTeam(entity.getScoreboardName());
        }
    }

    protected List<PackedScore> packPlayerScores() {
        List<PackedScore> packed = new ArrayList<>();
        scores.forEach((owner, perHolder) -> perHolder.forEach(
            (objective, score) -> packed.add(new PackedScore(owner, objective.getName(), score.pack()))));
        return packed;
    }

    protected void loadPlayerScore(PackedScore score) {
        Objective objective = getObjective(score.objective());
        if (objective != null) {
            Score target = new Score(score.score());
            scores.computeIfAbsent(score.owner(), k -> new LinkedHashMap<>()).put(objective, target);
        }
    }

    protected List<PlayerTeam.Packed> packPlayerTeams() {
        return teams.values().stream().map(PlayerTeam::pack).toList();
    }

    protected void loadPlayerTeam(PlayerTeam.Packed packed) {
        PlayerTeam team = addPlayerTeam(packed.name());
        packed.displayName().ifPresent(team::setDisplayName);
        team.setColor(packed.color());
        team.setAllowFriendlyFire(packed.allowFriendlyFire());
        team.setSeeFriendlyInvisibles(packed.seeFriendlyInvisibles());
        team.setPlayerPrefix(packed.memberNamePrefix());
        team.setPlayerSuffix(packed.memberNameSuffix());
        team.setNameTagVisibility(packed.nameTagVisibility());
        team.setDeathMessageVisibility(packed.deathMessageVisibility());
        team.setCollisionRule(packed.collisionRule());
        for (String member : packed.players()) {
            addPlayerToTeam(member, team);
        }
    }

    protected List<Objective.Packed> packObjectives() {
        return objectives.values().stream().map(Objective::pack).toList();
    }

    protected void loadObjective(Objective.Packed objective) {
        addObjective(objective.name(), objective.criteria(), objective.displayName(), objective.renderType(),
            objective.displayAutoUpdate(), objective.numberFormat().orElse(null));
    }

    protected Map<DisplaySlot, String> packDisplaySlots() {
        Map<DisplaySlot, String> packed = new EnumMap<>(DisplaySlot.class);
        displaySlots.forEach((slot, objective) -> packed.put(slot, objective.getName()));
        return packed;
    }

    public static record PackedScore(String owner, String objective, Score.Packed score) {
        public static final Codec<PackedScore> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.scores.Scoreboard.PackedScore.class, "CODEC");
    }
}
