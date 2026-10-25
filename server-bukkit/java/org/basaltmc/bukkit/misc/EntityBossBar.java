package org.basaltmc.bukkit.misc;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import org.basaltmc.bukkit.entity.EntityCore;
import org.bukkit.Bukkit;
import org.bukkit.boss.BarColor;
import org.bukkit.boss.BarFlag;
import org.bukkit.boss.BarStyle;
import org.bukkit.boss.BossBar;
import org.bukkit.entity.Player;

public final class EntityBossBar implements BossBar {
    private static final int kTitle = 0;
    private static final int kColor = 1;
    private static final int kStyle = 2;
    private static final int kDarken = 3;
    private static final int kMusic = 4;
    private static final int kFog = 5;
    private static final int kProgress = 6;
    private static final int kVisible = 7;

    private final EntityCore core;

    private EntityBossBar(EntityCore core) {
        this.core = core;
    }

    public static BossBar of(EntityCore core) {
        return new EntityBossBar(core);
    }

    private String[] facts() {
        if (core.call("bar_info") instanceof String[] facts && facts.length == 8) {
            return facts;
        }
        throw new IllegalStateException("The boss has no bar yet");
    }

    private void set(String field, String value) {
        core.call("bar_set", field, value);
    }

    @Override
    public String getTitle() {
        return Json.legacyOf(Json.component(facts()[kTitle]));
    }

    @Override
    public void setTitle(String title) {
        set("title", Json.of(Json.fromLegacy(title == null ? "" : title)));
    }

    @Override
    public BarColor getColor() {
        return BarColor.valueOf(facts()[kColor].toUpperCase(Locale.ROOT));
    }

    @Override
    public void setColor(BarColor color) {
        if (color == null) {
            throw new IllegalArgumentException("Color cannot be null");
        }
        set("color", color.name().toLowerCase(Locale.ROOT));
    }

    @Override
    public BarStyle getStyle() {
        String style = facts()[kStyle];
        return style.equals("progress") ? BarStyle.SOLID : BarStyle.valueOf("SEGMENTED_" + style.substring("notched_".length()));
    }

    @Override
    public void setStyle(BarStyle style) {
        if (style == null) {
            throw new IllegalArgumentException("Style cannot be null");
        }
        set("overlay", style == BarStyle.SOLID ? "progress" : "notched_" + style.name().substring("SEGMENTED_".length()));
    }

    private static String flagField(BarFlag flag) {
        return switch (flag) {
            case DARKEN_SKY -> "darken";
            case PLAY_BOSS_MUSIC -> "music";
            case CREATE_FOG -> "fog";
        };
    }

    @Override
    public void removeFlag(BarFlag flag) {
        set(flagField(flag), "0");
    }

    @Override
    public void addFlag(BarFlag flag) {
        set(flagField(flag), "1");
    }

    @Override
    public boolean hasFlag(BarFlag flag) {
        return "1".equals(facts()[switch (flag) {
            case DARKEN_SKY -> kDarken;
            case PLAY_BOSS_MUSIC -> kMusic;
            case CREATE_FOG -> kFog;
        }]);
    }

    @Override
    public void setProgress(double progress) {
        if (progress < 0.0 || progress > 1.0) {
            throw new IllegalArgumentException("Progress must be between 0.0 and 1.0 (" + progress + ")");
        }
        set("progress", Double.toString(progress));
    }

    @Override
    public double getProgress() {
        return Double.parseDouble(facts()[kProgress]);
    }

    @Override
    public void addPlayer(Player player) {
        core.call("bar_pin", (long) player.getEntityId(), true);
    }

    @Override
    public void removePlayer(Player player) {
        core.call("bar_pin", (long) player.getEntityId(), false);
    }

    @Override
    public void removeAll() {
        for (Player player : getPlayers()) {
            removePlayer(player);
        }
    }

    @Override
    public List<Player> getPlayers() {
        List<Player> out = new ArrayList<>();
        if (core.call("bar_players") instanceof String[] ids) {
            for (String id : ids) {
                for (Player player : Bukkit.getOnlinePlayers()) {
                    if (player.getEntityId() == Integer.parseInt(id)) {
                        out.add(player);
                    }
                }
            }
        }
        return out;
    }

    @Override
    public void setVisible(boolean visible) {
        set("visible", visible ? "1" : "0");
    }

    @Override
    public boolean isVisible() {
        return "1".equals(facts()[kVisible]);
    }

    @Override
    public void show() {
        setVisible(true);
    }

    @Override
    public void hide() {
        setVisible(false);
    }
}
