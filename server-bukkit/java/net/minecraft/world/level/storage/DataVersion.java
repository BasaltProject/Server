// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;

public record DataVersion(int version, String series) {
    public static final String MAIN_SERIES = "main";

    public DataVersion(int version) {
        this(version, MAIN_SERIES);
    }

    public boolean isSideSeries() {
        return !series.equals(MAIN_SERIES);
    }

    public boolean isCompatible(DataVersion other) {
        return series.equals(other.series);
    }
}
