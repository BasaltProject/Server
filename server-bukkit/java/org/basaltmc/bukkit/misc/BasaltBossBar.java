package org.basaltmc.bukkit.misc;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.MiscNatives;
import org.bukkit.Bukkit;
import org.bukkit.NamespacedKey;
import org.bukkit.boss.BarColor;
import org.bukkit.boss.BarFlag;
import org.bukkit.boss.BarStyle;
import org.bukkit.boss.KeyedBossBar;
import org.bukkit.entity.Player;

public final class BasaltBossBar implements KeyedBossBar {
    private static final int kId = 0;
    private static final int kKeyed = 1;
    private static final int kTitle = 2;
    private static final int kColor = 3;
    private static final int kStyle = 4;
    private static final int kDarken = 5;
    private static final int kMusic = 6;
    private static final int kFog = 7;
    private static final int kProgress = 8;
    private static final int kVisible = 9;

    private final String id;

    BasaltBossBar(String id) {
        this.id = id;
    }

    public static BasaltBossBar create(NamespacedKey key, String title, BarColor color, BarStyle style,
            BarFlag... flags) {
        if (color == null || style == null) {
            throw new IllegalArgumentException("Color and style cannot be null");
        }
        String[] fields = new String[10];
        fields[kId] = key == null ? "" : key.toString();
        fields[kKeyed] = key == null ? "0" : "1";
        fields[kTitle] = Json.of(Json.fromLegacy(title));
        fields[kColor] = color.name().toLowerCase(Locale.ROOT);
        fields[kStyle] = styleName(style);
        fields[kDarken] = "0";
        fields[kMusic] = "0";
        fields[kFog] = "0";
        for (BarFlag flag : flags) {
            fields[switch (flag) {
                case DARKEN_SKY -> kDarken;
                case PLAY_BOSS_MUSIC -> kMusic;
                case CREATE_FOG -> kFog;
            }] = "1";
        }
        fields[kProgress] = "1.0";
        fields[kVisible] = "1";
        String made = MiscNatives.bbCreate(fields);
        return made == null ? null : new BasaltBossBar(made);
    }

    public static BasaltBossBar get(NamespacedKey key) {
        String[] fields = MiscNatives.bbGet(key.toString());
        return fields == null || !"1".equals(fields[kKeyed]) ? null : new BasaltBossBar(key.toString());
    }

    public static List<KeyedBossBar> kept() {
        List<KeyedBossBar> out = new ArrayList<>();
        for (String id : MiscNatives.bbKept()) {
            out.add(new BasaltBossBar(id));
        }
        return out;
    }

    public static boolean remove(NamespacedKey key) {
        String[] fields = MiscNatives.bbGet(key.toString());
        return fields != null && "1".equals(fields[kKeyed]) && MiscNatives.bbRemove(key.toString());
    }

    private static String styleName(BarStyle style) {
        return style == BarStyle.SOLID ? "progress" : "notched_" + style.name().substring("SEGMENTED_".length());
    }

    String id() {
        return id;
    }

    private String[] fields() {
        String[] fields = MiscNatives.bbGet(id);
        if (fields == null) {
            throw new IllegalStateException("The boss bar was removed");
        }
        return fields;
    }

    private void write(String[] fields) {
        MiscNatives.bbUpdate(fields);
    }

    @Override
    public NamespacedKey getKey() {
        return NamespacedKey.fromString(id);
    }

    @Override
    public String getTitle() {
        return Json.legacyOf(Json.component(fields()[kTitle]));
    }

    @Override
    public void setTitle(String title) {
        String[] fields = fields();
        fields[kTitle] = Json.of(Json.fromLegacy(title));
        write(fields);
    }

    @Override
    public BarColor getColor() {
        return BarColor.valueOf(fields()[kColor].toUpperCase(Locale.ROOT));
    }

    @Override
    public void setColor(BarColor color) {
        if (color == null) {
            throw new IllegalArgumentException("Color cannot be null");
        }
        String[] fields = fields();
        fields[kColor] = color.name().toLowerCase(Locale.ROOT);
        write(fields);
    }

    @Override
    public BarStyle getStyle() {
        String style = fields()[kStyle];
        return style.equals("progress") ? BarStyle.SOLID : BarStyle.valueOf("SEGMENTED_" + style.substring("notched_".length()));
    }

    @Override
    public void setStyle(BarStyle style) {
        if (style == null) {
            throw new IllegalArgumentException("Style cannot be null");
        }
        String[] fields = fields();
        fields[kStyle] = styleName(style);
        write(fields);
    }

    private static int flagField(BarFlag flag) {
        return switch (flag) {
            case DARKEN_SKY -> kDarken;
            case PLAY_BOSS_MUSIC -> kMusic;
            case CREATE_FOG -> kFog;
        };
    }

    @Override
    public void removeFlag(BarFlag flag) {
        String[] fields = fields();
        fields[flagField(flag)] = "0";
        write(fields);
    }

    @Override
    public void addFlag(BarFlag flag) {
        String[] fields = fields();
        fields[flagField(flag)] = "1";
        write(fields);
    }

    @Override
    public boolean hasFlag(BarFlag flag) {
        return "1".equals(fields()[flagField(flag)]);
    }

    @Override
    public void setProgress(double progress) {
        if (progress < 0.0 || progress > 1.0) {
            throw new IllegalArgumentException("Progress must be between 0.0 and 1.0 (" + progress + ")");
        }
        String[] fields = fields();
        fields[kProgress] = Double.toString(progress);
        write(fields);
    }

    @Override
    public double getProgress() {
        return Double.parseDouble(fields()[kProgress]);
    }

    @Override
    public void addPlayer(Player player) {
        fields();
        MiscNatives.bbAddPlayer(id, player.getUniqueId().toString());
    }

    @Override
    public void removePlayer(Player player) {
        fields();
        MiscNatives.bbRemovePlayer(id, player.getUniqueId().toString());
    }

    @Override
    public void removeAll() {
        fields();
        MiscNatives.bbClear(id);
    }

    @Override
    public List<Player> getPlayers() {
        List<Player> out = new ArrayList<>();
        for (String uuid : MiscNatives.bbPlayers(id)) {
            Player player = Bukkit.getPlayer(java.util.UUID.fromString(uuid));
            if (player != null) {
                out.add(player);
            }
        }
        return out;
    }

    @Override
    public void setVisible(boolean visible) {
        String[] fields = fields();
        fields[kVisible] = visible ? "1" : "0";
        write(fields);
    }

    @Override
    public boolean isVisible() {
        return "1".equals(fields()[kVisible]);
    }

    @Override
    public void show() {
        setVisible(true);
    }

    @Override
    public void hide() {
        setVisible(false);
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltBossBar bar && bar.id.equals(id);
    }

    @Override
    public int hashCode() {
        return id.hashCode();
    }
}
