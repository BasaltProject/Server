package org.basaltmc.bukkit.misc;

import io.papermc.paper.scoreboard.numbers.NumberFormat;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.MiscNatives;
import org.bukkit.OfflinePlayer;
import org.bukkit.entity.Entity;
import org.bukkit.scoreboard.Criteria;
import org.bukkit.scoreboard.DisplaySlot;
import org.bukkit.scoreboard.Objective;
import org.bukkit.scoreboard.RenderType;
import org.bukkit.scoreboard.Score;
import org.bukkit.scoreboard.Scoreboard;

public final class BasaltObjective implements Objective {
    private static final int kName = 0;
    private static final int kCriteria = 1;
    private static final int kDisplay = 2;
    private static final int kHearts = 3;
    private static final int kAuto = 4;
    private static final int kFormat = 5;
    private static final int kFormatText = 6;
    private static final int kReadOnly = 7;

    private final BasaltScoreboard board;
    private final String name;

    BasaltObjective(BasaltScoreboard board, String name) {
        this.board = board;
        this.name = name;
    }

    String[] fields() {
        String[] fields = MiscNatives.sbObjective(board.handle(), name);
        if (fields == null) {
            throw new IllegalStateException("Unregistered scoreboard component");
        }
        return fields;
    }

    private void write(String[] fields) {
        MiscNatives.sbUpdateObjective(board.handle(), fields);
    }

    BasaltScoreboard board() {
        return board;
    }

    @Override
    public String getName() {
        fields();
        return name;
    }

    @Override
    public Component displayName() {
        return Json.component(fields()[kDisplay]);
    }

    @Override
    public void displayName(Component displayName) {
        String[] fields = fields();
        fields[kDisplay] = Json.of(displayName == null ? Component.text(name) : displayName);
        write(fields);
    }

    @Override
    public String getDisplayName() {
        return Json.legacyOf(displayName());
    }

    @Override
    public void setDisplayName(String displayName) {
        if (displayName == null) {
            throw new IllegalArgumentException("Display name cannot be null");
        }
        displayName(Json.fromLegacy(displayName));
    }

    @Override
    public String getCriteria() {
        return fields()[kCriteria];
    }

    @Override
    public Criteria getTrackedCriteria() {
        return new BasaltCriteria(fields()[kCriteria]);
    }

    @Override
    public boolean isModifiable() {
        return !"1".equals(fields()[kReadOnly]);
    }

    @Override
    public Scoreboard getScoreboard() {
        return board;
    }

    @Override
    public void unregister() {
        fields();
        MiscNatives.sbRemoveObjective(board.handle(), name);
    }

    @Override
    public void setDisplaySlot(DisplaySlot slot) {
        fields();
        for (DisplaySlot each : DisplaySlot.values()) {
            if (name.equals(MiscNatives.sbDisplayed(board.handle(), each.ordinal())) && each != slot) {
                MiscNatives.sbDisplay(board.handle(), each.ordinal(), null);
            }
        }
        if (slot != null) {
            MiscNatives.sbDisplay(board.handle(), slot.ordinal(), name);
        }
    }

    @Override
    public DisplaySlot getDisplaySlot() {
        fields();
        for (DisplaySlot each : DisplaySlot.values()) {
            if (name.equals(MiscNatives.sbDisplayed(board.handle(), each.ordinal()))) {
                return each;
            }
        }
        return null;
    }

    @Override
    public void setRenderType(RenderType renderType) {
        if (renderType == null) {
            throw new IllegalArgumentException("RenderType cannot be null");
        }
        String[] fields = fields();
        fields[kHearts] = renderType == RenderType.HEARTS ? "1" : "0";
        write(fields);
    }

    @Override
    public RenderType getRenderType() {
        return "1".equals(fields()[kHearts]) ? RenderType.HEARTS : RenderType.INTEGER;
    }

    @Override
    public Score getScore(OfflinePlayer player) {
        fields();
        return new BasaltScore(this, BasaltScoreboard.entryOf(player));
    }

    @Override
    public Score getScore(String entry) {
        fields();
        if (entry == null) {
            throw new IllegalArgumentException("Entry cannot be null");
        }
        return new BasaltScore(this, entry);
    }

    @Override
    public Score getScoreFor(Entity entity) {
        fields();
        return new BasaltScore(this, BasaltScoreboard.entryOf(entity));
    }

    @Override
    public boolean willAutoUpdateDisplay() {
        return "1".equals(fields()[kAuto]);
    }

    @Override
    public void setAutoUpdateDisplay(boolean autoUpdateDisplay) {
        String[] fields = fields();
        fields[kAuto] = autoUpdateDisplay ? "1" : "0";
        write(fields);
    }

    @Override
    public NumberFormat numberFormat() {
        String[] fields = fields();
        return BoardNumbers.parse(fields[kFormat], fields[kFormatText]);
    }

    @Override
    public void numberFormat(NumberFormat format) {
        String[] fields = fields();
        fields[kFormat] = BoardNumbers.kind(format);
        fields[kFormatText] = BoardNumbers.text(format);
        write(fields);
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltObjective objective && objective.board.equals(board)
            && objective.name.equals(name);
    }

    @Override
    public int hashCode() {
        return name.hashCode() * 31 + board.hashCode();
    }
}
