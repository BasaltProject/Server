package org.basaltmc.bukkit.misc;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import net.kyori.adventure.bossbar.BossBar;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.MiscNatives;

/** Adventure's boss bars shown to players: each is mirrored by a bar of the server, and follows the original. */
public final class AdventureBars {
    private AdventureBars() {}

    private static final class Mirror implements BossBar.Listener {
        final BossBar bar;
        final String id;
        final Set<UUID> viewers = new HashSet<>();

        Mirror(BossBar bar) {
            this.bar = bar;
            String[] fields = new String[10];
            fields[0] = "";
            fields[1] = "0";
            fields[2] = Json.of(bar.name());
            fields[3] = colorName(bar.color());
            fields[4] = overlayName(bar.overlay());
            fields[5] = bar.hasFlag(BossBar.Flag.DARKEN_SCREEN) ? "1" : "0";
            fields[6] = bar.hasFlag(BossBar.Flag.PLAY_BOSS_MUSIC) ? "1" : "0";
            fields[7] = bar.hasFlag(BossBar.Flag.CREATE_WORLD_FOG) ? "1" : "0";
            fields[8] = Float.toString(bar.progress());
            fields[9] = "1";
            this.id = MiscNatives.bbCreate(fields);
            bar.addListener(this);
        }

        private void push() {
            String[] fields = MiscNatives.bbGet(id);
            if (fields == null) {
                return;
            }
            fields[2] = Json.of(bar.name());
            fields[3] = colorName(bar.color());
            fields[4] = overlayName(bar.overlay());
            fields[5] = bar.hasFlag(BossBar.Flag.DARKEN_SCREEN) ? "1" : "0";
            fields[6] = bar.hasFlag(BossBar.Flag.PLAY_BOSS_MUSIC) ? "1" : "0";
            fields[7] = bar.hasFlag(BossBar.Flag.CREATE_WORLD_FOG) ? "1" : "0";
            fields[8] = Float.toString(bar.progress());
            MiscNatives.bbUpdate(fields);
        }

        @Override
        public void bossBarNameChanged(BossBar bar, Component oldName, Component newName) {
            push();
        }

        @Override
        public void bossBarProgressChanged(BossBar bar, float oldProgress, float newProgress) {
            push();
        }

        @Override
        public void bossBarColorChanged(BossBar bar, BossBar.Color oldColor, BossBar.Color newColor) {
            push();
        }

        @Override
        public void bossBarOverlayChanged(BossBar bar, BossBar.Overlay oldOverlay, BossBar.Overlay newOverlay) {
            push();
        }

        @Override
        public void bossBarFlagsChanged(BossBar bar, Set<BossBar.Flag> flagsAdded, Set<BossBar.Flag> flagsRemoved) {
            push();
        }
    }

    private static final Map<BossBar, Mirror> kMirrors = new HashMap<>();

    private static String colorName(BossBar.Color color) {
        return color.name().toLowerCase(Locale.ROOT);
    }

    private static String overlayName(BossBar.Overlay overlay) {
        return switch (overlay) {
            case PROGRESS -> "progress";
            case NOTCHED_6 -> "notched_6";
            case NOTCHED_10 -> "notched_10";
            case NOTCHED_12 -> "notched_12";
            case NOTCHED_20 -> "notched_20";
        };
    }

    public static synchronized java.util.List<BossBar> shownTo(UUID player) {
        java.util.List<BossBar> out = new java.util.ArrayList<>();
        for (Map.Entry<BossBar, Mirror> entry : kMirrors.entrySet()) {
            if (entry.getValue().viewers.contains(player)) {
                out.add(entry.getKey());
            }
        }
        return out;
    }

    public static synchronized void show(UUID player, BossBar bar) {
        Mirror mirror = org.basaltmc.bukkit.facade.Memo.get(kMirrors, bar, Mirror::new);
        if (mirror.id != null && mirror.viewers.add(player)) {
            MiscNatives.bbAddPlayer(mirror.id, player.toString());
        }
    }

    public static synchronized void hide(UUID player, BossBar bar) {
        Mirror mirror = kMirrors.get(bar);
        if (mirror == null || !mirror.viewers.remove(player)) {
            return;
        }
        MiscNatives.bbRemovePlayer(mirror.id, player.toString());
        if (mirror.viewers.isEmpty()) {
            kMirrors.remove(bar);
            bar.removeListener(mirror);
            MiscNatives.bbRemove(mirror.id);
        }
    }
}
