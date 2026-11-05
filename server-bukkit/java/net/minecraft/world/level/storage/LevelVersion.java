// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;

import com.mojang.serialization.Dynamic;

public class LevelVersion {
    private final int levelDataVersion;
    private final long lastPlayed;
    private final String minecraftVersionName;
    private final DataVersion minecraftVersion;
    private final boolean snapshot;

    private LevelVersion(int levelDataVersion, long lastPlayed, String minecraftVersionName, int minecraftVersion, String series, boolean snapshot) {
        this.levelDataVersion = levelDataVersion;
        this.lastPlayed = lastPlayed;
        this.minecraftVersionName = minecraftVersionName;
        this.minecraftVersion = new DataVersion(minecraftVersion, series);
        this.snapshot = snapshot;
    }

    public static LevelVersion parse(Dynamic<?> input) {
        Dynamic<?> version = input.get("Version").orElseEmptyMap();
        return new LevelVersion(input.get("version").asInt(0), input.get("LastPlayed").asLong(0L),
            version.get("Name").asString(net.minecraft.SharedConstants.getCurrentVersion().name()), input.get("DataVersion").asInt(0),
            input.get("DataVersionSeries").asString(DataVersion.MAIN_SERIES), version.get("Snapshot").asBoolean(false));
    }

    public int levelDataVersion() {
        return levelDataVersion;
    }

    public long lastPlayed() {
        return lastPlayed;
    }

    public String minecraftVersionName() {
        return minecraftVersionName;
    }

    public DataVersion minecraftVersion() {
        return minecraftVersion;
    }

    public boolean snapshot() {
        return snapshot;
    }
}
