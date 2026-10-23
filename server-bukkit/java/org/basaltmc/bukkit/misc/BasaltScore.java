package org.basaltmc.bukkit.misc;

import io.papermc.paper.scoreboard.numbers.NumberFormat;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.MiscNatives;
import org.bukkit.OfflinePlayer;
import org.bukkit.scoreboard.Objective;
import org.bukkit.scoreboard.Score;
import org.bukkit.scoreboard.Scoreboard;

public final class BasaltScore implements Score {
    private final BasaltObjective objective;
    private final String entry;

    BasaltScore(BasaltObjective objective, String entry) {
        this.objective = objective;
        this.entry = entry;
    }

    private int handle() {
        objective.fields();
        return objective.board().handle();
    }

    @Override
    public OfflinePlayer getPlayer() {
        return BasaltScoreboard.playerOf(entry);
    }

    @Override
    public String getEntry() {
        return entry;
    }

    @Override
    public Objective getObjective() {
        return objective;
    }

    @Override
    public int getScore() {
        long value = MiscNatives.sbScore(handle(), entry, objective.getName());
        return value == Long.MIN_VALUE ? 0 : (int) value;
    }

    @Override
    public void setScore(int score) {
        if (!objective.isModifiable()) {
            throw new IllegalArgumentException("The score of a read-only criteria cannot be changed");
        }
        MiscNatives.sbSetScore(handle(), entry, objective.getName(), score);
    }

    @Override
    public boolean isScoreSet() {
        return MiscNatives.sbScore(handle(), entry, objective.getName()) != Long.MIN_VALUE;
    }

    @Override
    public Scoreboard getScoreboard() {
        return objective.board();
    }

    @Override
    public void resetScore() {
        MiscNatives.sbResetScore(handle(), entry, objective.getName());
    }

    @Override
    public boolean isTriggerable() {
        return MiscNatives.sbTriggerable(handle(), entry, objective.getName());
    }

    @Override
    public void setTriggerable(boolean triggerable) {
        if (!"trigger".equals(objective.getCriteria())) {
            throw new IllegalStateException("This criteria is not a trigger");
        }
        MiscNatives.sbSetTriggerable(handle(), entry, objective.getName(), triggerable);
    }

    @Override
    public Component customName() {
        String[] shown = MiscNatives.sbScoreDisplay(handle(), entry, objective.getName());
        return shown[0] == null ? null : Json.component(shown[0]);
    }

    @Override
    public void customName(Component customName) {
        String[] shown = MiscNatives.sbScoreDisplay(handle(), entry, objective.getName());
        shown[0] = customName == null ? null : Json.of(customName);
        MiscNatives.sbSetScoreDisplay(handle(), entry, objective.getName(), shown);
    }

    @Override
    public NumberFormat numberFormat() {
        String[] shown = MiscNatives.sbScoreDisplay(handle(), entry, objective.getName());
        return BoardNumbers.parse(shown[1], shown[2]);
    }

    @Override
    public void numberFormat(NumberFormat format) {
        String[] shown = MiscNatives.sbScoreDisplay(handle(), entry, objective.getName());
        shown[1] = BoardNumbers.kind(format);
        shown[2] = BoardNumbers.text(format);
        MiscNatives.sbSetScoreDisplay(handle(), entry, objective.getName(), shown);
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltScore score && score.objective.equals(objective) && score.entry.equals(entry);
    }

    @Override
    public int hashCode() {
        return entry.hashCode() * 31 + objective.hashCode();
    }
}
