package org.basaltmc.bukkit.misc;

import org.basaltmc.bukkit.MiscNatives;
import org.bukkit.scoreboard.Scoreboard;
import org.bukkit.scoreboard.ScoreboardManager;

public final class BasaltScoreboardManager implements ScoreboardManager {
    private final BasaltScoreboard main = new BasaltScoreboard(0);

    @Override
    public Scoreboard getMainScoreboard() {
        return main;
    }

    @Override
    public Scoreboard getNewScoreboard() {
        int handle = MiscNatives.sbCreate();
        if (handle < 0) {
            throw new IllegalStateException("The server has no scoreboards");
        }
        return new BasaltScoreboard(handle);
    }
}
