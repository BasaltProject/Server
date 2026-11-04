// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.sounds;
public enum SoundSource {
    MASTER, MUSIC, RECORDS, WEATHER, BLOCKS, HOSTILE, NEUTRAL, PLAYERS, AMBIENT, VOICE, UI;
;
    public String getName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
}
