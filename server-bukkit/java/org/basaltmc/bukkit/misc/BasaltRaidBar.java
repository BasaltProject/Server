package org.basaltmc.bukkit.misc;

import java.util.ArrayList;
import java.util.List;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.WorldEnvNatives;
import org.bukkit.Bukkit;
import org.bukkit.boss.BarColor;
import org.bukkit.boss.BarFlag;
import org.bukkit.boss.BarStyle;
import org.bukkit.boss.BossBar;
import org.bukkit.entity.Player;

public final class BasaltRaidBar implements BossBar {
    private static final BarColor[] kColors = {BarColor.PINK, BarColor.BLUE, BarColor.RED, BarColor.GREEN,
        BarColor.YELLOW, BarColor.PURPLE, BarColor.WHITE};
    private static final BarStyle[] kStyles = {BarStyle.SOLID, BarStyle.SEGMENTED_6, BarStyle.SEGMENTED_10,
        BarStyle.SEGMENTED_12, BarStyle.SEGMENTED_20};

    private final String world;
    private final int id;

    public BasaltRaidBar(String world, int id) {
        this.world = world;
        this.id = id;
    }

    private String[] facts() {
        String[] facts = WorldEnvNatives.raidBar(world, id);
        if (facts == null) {
            throw new IllegalStateException("The raid is over");
        }
        return facts;
    }

    private void control(String action, String text, int value) {
        facts();
        WorldEnvNatives.raidBarControl(world, id, action, text, value);
    }

    private static int bit(BarFlag flag) {
        return switch (flag) {
            case DARKEN_SKY -> 1;
            case PLAY_BOSS_MUSIC -> 2;
            case CREATE_FOG -> 4;
        };
    }

    @Override
    public String getTitle() {
        return Json.legacyOf(Json.component(facts()[0]));
    }

    @Override
    public void setTitle(String title) {
        control("bar_title", Json.of(Json.fromLegacy(title)), 0);
    }

    @Override
    public BarColor getColor() {
        return kColors[Integer.parseInt(facts()[1])];
    }

    @Override
    public void setColor(BarColor color) {
        if (color == null) {
            throw new IllegalArgumentException("Color cannot be null");
        }
        control("bar_color", null, color.ordinal());
    }

    @Override
    public BarStyle getStyle() {
        return kStyles[Integer.parseInt(facts()[2])];
    }

    @Override
    public void setStyle(BarStyle style) {
        if (style == null) {
            throw new IllegalArgumentException("Style cannot be null");
        }
        control("bar_style", null, style.ordinal());
    }

    @Override
    public void removeFlag(BarFlag flag) {
        control("bar_flags", null, Integer.parseInt(facts()[3]) & ~bit(flag));
    }

    @Override
    public void addFlag(BarFlag flag) {
        control("bar_flags", null, Integer.parseInt(facts()[3]) | bit(flag));
    }

    @Override
    public boolean hasFlag(BarFlag flag) {
        return (Integer.parseInt(facts()[3]) & bit(flag)) != 0;
    }

    @Override
    public void setProgress(double progress) {
        if (progress < 0.0 || progress > 1.0) {
            throw new IllegalArgumentException("Progress must be between 0.0 and 1.0 (" + progress + ")");
        }
        control("bar_progress", null, (int) Math.round(progress * 10000.0));
    }

    @Override
    public double getProgress() {
        return Double.parseDouble(facts()[4]);
    }

    @Override
    public void addPlayer(Player player) {
        control("bar_add", null, player.getEntityId());
    }

    @Override
    public void removePlayer(Player player) {
        control("bar_remove", null, player.getEntityId());
    }

    @Override
    public void removeAll() {
        control("bar_clear", null, 0);
    }

    @Override
    public List<Player> getPlayers() {
        String csv = facts()[6];
        List<Player> out = new ArrayList<>();
        for (String entityId : csv.isEmpty() ? new String[0] : csv.split(",")) {
            int wanted = Integer.parseInt(entityId);
            for (Player player : Bukkit.getOnlinePlayers()) {
                if (player.getEntityId() == wanted) {
                    out.add(player);
                }
            }
        }
        return out;
    }

    @Override
    public void setVisible(boolean visible) {
        control("bar_visible", null, visible ? 1 : 0);
    }

    @Override
    public boolean isVisible() {
        return "1".equals(facts()[5]);
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
