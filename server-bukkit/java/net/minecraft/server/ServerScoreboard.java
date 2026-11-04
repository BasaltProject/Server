// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

import net.minecraft.world.scores.DisplaySlot;
import net.minecraft.world.scores.Objective;
import net.minecraft.world.scores.PlayerTeam;
import net.minecraft.world.scores.Score;
import net.minecraft.world.scores.ScoreHolder;
import net.minecraft.world.scores.Scoreboard;
import net.minecraft.world.scores.ScoreboardSaveData;
import net.minecraft.world.scores.criteria.ObjectiveCriteria;
import org.bukkit.Bukkit;
import org.bukkit.scoreboard.RenderType;

/** The vanilla scoreboard view over the native main board: every change is written through to it. */
public class ServerScoreboard extends Scoreboard {
    private final org.bukkit.scoreboard.Scoreboard main;
    private boolean pulling;

    public ServerScoreboard(MinecraftServer server) {
        this.main = Bukkit.getScoreboardManager().getMainScoreboard();
        pull();
    }

    private void pull() {
        pulling = true;
        try {
            for (org.bukkit.scoreboard.Objective each : main.getObjectives()) {
                ObjectiveCriteria criteria = ObjectiveCriteria.byName(each.getTrackedCriteria().getName()).orElse(ObjectiveCriteria.DUMMY);
                net.minecraft.network.chat.Component shown = net.minecraft.network.chat.Component.literal(each.getName());
                ObjectiveCriteria.RenderType render = each.getRenderType() == RenderType.HEARTS ? ObjectiveCriteria.RenderType.HEARTS : ObjectiveCriteria.RenderType.INTEGER;
                Objective made = addObjective(each.getName(), criteria, shown, render, true, null);
                for (String entry : main.getEntries()) {
                    org.bukkit.scoreboard.Score score = each.getScore(entry);
                    if (score.isScoreSet()) {
                        getOrCreatePlayerScore(ScoreHolder.forNameOnly(entry), made, true).set(score.getScore());
                    }
                }
            }
            for (org.bukkit.scoreboard.Team each : main.getTeams()) {
                PlayerTeam team = addPlayerTeam(each.getName());
                for (String entry : each.getEntries()) {
                    addPlayerToTeam(entry, team);
                }
            }
        } finally {
            pulling = false;
        }
    }

    public void load(ScoreboardSaveData.Packed data) {
    }

    private org.bukkit.scoreboard.Objective nativeObjective(Objective objective) {
        return main.getObjective(objective.getName());
    }

    @Override
    protected void onScoreChanged(ScoreHolder owner, Objective objective, Score score) {
        org.bukkit.scoreboard.Objective target = pulling ? null : nativeObjective(objective);
        if (target != null) {
            target.getScore(owner.getScoreboardName()).setScore(score.value());
        }
    }

    @Override
    public void onPlayerRemoved(ScoreHolder player) {
        if (!pulling) {
            main.resetScores(player.getScoreboardName());
        }
    }

    @Override
    public void onPlayerScoreRemoved(ScoreHolder player, Objective objective) {
        org.bukkit.scoreboard.Objective target = pulling ? null : nativeObjective(objective);
        if (target != null) {
            target.getScore(player.getScoreboardName()).resetScore();
        }
    }

    @Override
    public void setDisplayObjective(DisplaySlot slot, Objective objective) {
        super.setDisplayObjective(slot, objective);
        org.bukkit.scoreboard.DisplaySlot mapped = switch (slot) {
            case LIST -> org.bukkit.scoreboard.DisplaySlot.PLAYER_LIST;
            case SIDEBAR -> org.bukkit.scoreboard.DisplaySlot.SIDEBAR;
            case BELOW_NAME -> org.bukkit.scoreboard.DisplaySlot.BELOW_NAME;
            default -> null;
        };
        if (mapped == null || pulling) {
            return;
        }
        if (objective == null) {
            main.clearSlot(mapped);
        } else if (nativeObjective(objective) != null) {
            nativeObjective(objective).setDisplaySlot(mapped);
        }
    }

    @Override
    public void onObjectiveAdded(Objective objective) {
        if (!pulling && nativeObjective(objective) == null) {
            RenderType render = objective.getRenderType() == ObjectiveCriteria.RenderType.HEARTS ? RenderType.HEARTS : RenderType.INTEGER;
            main.registerNewObjective(objective.getName(), objective.getCriteria().getName(), objective.getDisplayName().asAdventure(), render);
        }
    }

    @Override
    public void onObjectiveChanged(Objective objective) {
        org.bukkit.scoreboard.Objective target = pulling ? null : nativeObjective(objective);
        if (target != null) {
            target.displayName(objective.getDisplayName().asAdventure());
            target.setRenderType(objective.getRenderType() == ObjectiveCriteria.RenderType.HEARTS ? RenderType.HEARTS : RenderType.INTEGER);
        }
    }

    @Override
    public void onObjectiveRemoved(Objective objective) {
        org.bukkit.scoreboard.Objective target = pulling ? null : nativeObjective(objective);
        if (target != null) {
            target.unregister();
        }
    }

    @Override
    public void onTeamAdded(PlayerTeam team) {
        if (!pulling && main.getTeam(team.getName()) == null) {
            main.registerNewTeam(team.getName());
        }
    }

    @Override
    public void onTeamChanged(PlayerTeam team) {
        org.bukkit.scoreboard.Team target = pulling ? null : main.getTeam(team.getName());
        if (target == null) {
            return;
        }
        target.prefix(team.getPlayerPrefix().asAdventure());
        target.suffix(team.getPlayerSuffix().asAdventure());
        for (String entry : target.getEntries()) {
            if (!team.getPlayers().contains(entry)) {
                target.removeEntry(entry);
            }
        }
        for (String entry : team.getPlayers()) {
            if (!target.hasEntry(entry)) {
                target.addEntry(entry);
            }
        }
    }

    @Override
    public void onTeamRemoved(PlayerTeam team) {
        org.bukkit.scoreboard.Team target = pulling ? null : main.getTeam(team.getName());
        if (target != null) {
            target.unregister();
        }
    }

    public java.util.List<net.minecraft.network.protocol.Packet<?>> getStartTrackingPackets(Objective objective) {
        return java.util.List.of();
    }

    public void startTrackingObjective(Objective objective, net.minecraft.server.level.ServerPlayer player) {
    }

    public void stopTrackingObjective(Objective objective, net.minecraft.server.level.ServerPlayer player) {
    }
}
