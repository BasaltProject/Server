package net.minecraft.world.scores;
public enum TeamColor {
    BLACK,
    DARK_BLUE,
    DARK_GREEN,
    DARK_AQUA,
    DARK_RED,
    DARK_PURPLE,
    GOLD,
    GRAY,
    DARK_GRAY,
    BLUE,
    GREEN,
    AQUA,
    RED,
    LIGHT_PURPLE,
    YELLOW,
    WHITE;
    public int id() {
        return ordinal();
    }
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
    public static TeamColor byName(String name) {
        if (name == null)
            return null;
        try {
            return valueOf(name.toUpperCase(java.util.Locale.ROOT));
        } catch (IllegalArgumentException e) {
            return null;
        }
    }
}
